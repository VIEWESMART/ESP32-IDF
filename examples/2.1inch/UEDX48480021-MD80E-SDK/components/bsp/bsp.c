/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "iot_knob.h"

#include "bsp/esp-bsp.h"
#include "bsp_err_check.h"
#include "bsp_sub_board.h"

static const char *TAG = "bsp";

static lv_disp_draw_buf_t disp_buf;                 // Contains internal graphic buffer(s) called draw buffer(s)
static lv_disp_drv_t disp_drv;                      // Contains LCD panel handle and callback functions
static SemaphoreHandle_t lvgl_mux;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle;

/**********************************************************************************************************
 *
 * Display Function
 *
 **********************************************************************************************************/
#if CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
// This function is located in ROM (also see esp_rom/${target}/ld/${target}.rom.ld)
extern int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

static inline void *lv_port_flush_get_next_buf(void *buf)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    lv_disp_draw_buf_t *draw_buf = disp->driver->draw_buf;
    return (buf == draw_buf->buf1) ? draw_buf->buf2 : draw_buf->buf1;
}

typedef struct {
    uint16_t inv_p;
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];
    lv_area_t inv_areas[LV_INV_BUF_SIZE];
} lv_port_dirty_area_t;

static lv_port_dirty_area_t dirty_area;

static void lv_port_flush_dirty_save(lv_port_dirty_area_t *dirty_area)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    dirty_area->inv_p = disp->inv_p;
    for (int i = 0; i < disp->inv_p; i++) {
        dirty_area->inv_area_joined[i] = disp->inv_area_joined[i];
        dirty_area->inv_areas[i] = disp->inv_areas[i];
    }
}

/**
 * @brief Copy dirty area
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 *
 */
static void lv_port_flush_dirty_copy(void *dst, void *src, lv_port_dirty_area_t *dirty_area)
{
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    lv_coord_t x_start, x_end, y_start, y_end;
    uint32_t copy_bytes_per_line;
    uint32_t bytes_to_flush;
    uint32_t h_res = disp_refr->driver->hor_res;
    uint32_t bytes_per_line = h_res * 2;
    uint8_t *from, *to;
    uint8_t *flush_ptr;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        /* Refresh the unjoined areas*/
        if (disp_refr->inv_area_joined[i] == 0) {
            x_start = disp_refr->inv_areas[i].x1;
            x_end = disp_refr->inv_areas[i].x2 + 1;
            y_start = disp_refr->inv_areas[i].y1;
            y_end = disp_refr->inv_areas[i].y2 + 1;

            copy_bytes_per_line = (x_end - x_start) * 2;
            from = src + (y_start * h_res + x_start) * 2;
            to = dst + (y_start * h_res + x_start) * 2;
            for (int y = y_start; y < y_end; y++) {
                memcpy(to, from, copy_bytes_per_line);
                from += bytes_per_line;
                to += bytes_per_line;
            }
            bytes_to_flush = (y_end - y_start) * bytes_per_line;
            flush_ptr = dst + y_start * bytes_per_line;

            Cache_WriteBack_Addr((uint32_t)(flush_ptr), bytes_to_flush);
        }
    }
}

typedef enum {
    FLUSH_STATUS_PART,
    FLUSH_STATUS_FULL
} lv_port_flush_status_t;

typedef enum {
    FLUSH_PROBE_PART_COPY,
    FLUSH_PROBE_SKIP_COPY,
    FLUSH_PROBE_FULL_COPY,
} lv_port_flush_probe_t;

/**
 * @brief Probe dirty area to copy
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 *
 */
lv_port_flush_probe_t lv_port_flush_copy_probe(void)
{
    static lv_port_flush_status_t prev_status = FLUSH_PROBE_PART_COPY;
    lv_port_flush_status_t cur_status;
    uint8_t probe_result;
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    uint32_t flush_ver = 0;
    uint32_t flush_hor = 0;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        if (disp_refr->inv_area_joined[i] == 0) {
            flush_ver = (disp_refr->inv_areas[i].y2 + 1 - disp_refr->inv_areas[i].y1);
            flush_hor = (disp_refr->inv_areas[i].x2 + 1 - disp_refr->inv_areas[i].x1);
            break;
        }
    }
    /* Check if the current full screen refreshes */
    cur_status = ((flush_ver == disp_drv.ver_res) && (flush_hor == disp_drv.hor_res)) ? (FLUSH_STATUS_FULL) : (FLUSH_STATUS_PART);

    if (prev_status == FLUSH_STATUS_FULL) {
        if ((cur_status == FLUSH_STATUS_PART)) {
            probe_result = FLUSH_PROBE_FULL_COPY;
        } else {
            probe_result = FLUSH_PROBE_SKIP_COPY;
        }
    } else {
        probe_result = FLUSH_PROBE_PART_COPY;
    }
    prev_status = cur_status;

    return probe_result;
}
#endif

#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH && CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 3
static void *lvgl_port_rgb_last_buf = NULL;
static void *lvgl_port_rgb_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;
#endif

static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
#if CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 3
    drv->draw_buf->buf1 = color_map;
    drv->draw_buf->buf2 = lvgl_port_flush_next_buf;
    lvgl_port_flush_next_buf = color_map;
#endif
    /* Due to full-refresh mode, here we just swtich pointer of frame buffer rather than draw bitmap */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
#if CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 3
    lvgl_port_rgb_next_buf = color_map;
#else
    /* Waiting for the current frame buffer to complete transmission */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#endif
#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
    lv_port_flush_probe_t probe_result;
    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        if (drv->full_refresh) {
            drv->full_refresh = 0;
            /* Due to direct-mode, here we just swtich driver's pointer of frame buffer rather than draw bitmap */
            esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
            /* Waiting for the current frame buffer to complete transmission */
            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            lv_port_flush_dirty_copy(lv_port_flush_get_next_buf(color_map), color_map, &dirty_area);
            drv->draw_buf->buf_act = (color_map == drv->draw_buf->buf1) ? drv->draw_buf->buf2 : drv->draw_buf->buf1;
        } else {
            /* Probe and copy dirty area from the current LVGL's frame buffer to the next LVGL's frame buffer */
            probe_result = lv_port_flush_copy_probe();
            if (probe_result == FLUSH_PROBE_FULL_COPY) {
                lv_port_flush_dirty_save(&dirty_area);
                /* Set LVGL full-refresh flag and force to refresh whole screen */
                drv->full_refresh = 1;
                lv_disp_flush_ready(drv);
                lv_refr_now(_lv_refr_get_disp_refreshing());
            } else {
                /* Due to direct-mode, here we just swtich driver's pointer of frame buffer rather than draw bitmap */
                esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
                /* Waiting for the current frame buffer to complete transmission */
                ulTaskNotifyValueClear(NULL, ULONG_MAX);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (probe_result == FLUSH_PROBE_PART_COPY) {
                    lv_port_flush_dirty_save(&dirty_area);
                    lv_port_flush_dirty_copy(lv_port_flush_get_next_buf(color_map), color_map, &dirty_area);
                }
            }
        }
    }
#else
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
#endif

    lv_disp_flush_ready(drv);
}

static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    case LV_DISP_ROT_90:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_180:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_270:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    }
}

static void lvgl_port_tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static esp_err_t lvgl_port_tick_init(void)
{
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_port_tick_increment,
        .name = "LVGL tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    BSP_ERROR_CHECK_RETURN_ERR(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    while (1) {
        bsp_display_lock(0);
        uint32_t task_delay_ms = lv_timer_handler();
        bsp_display_unlock();
        if (task_delay_ms > 500) {
            task_delay_ms = 500;
        } else if (task_delay_ms < CONFIG_BSP_DISPLAY_LVGL_TASK_DELAY) {
            task_delay_ms = CONFIG_BSP_DISPLAY_LVGL_TASK_DELAY;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static lv_disp_t *lvgl_port_display_init(void)
{
    esp_lcd_panel_handle_t panel_handle = bsp_lcd_init(NULL);
    BSP_NULL_CHECK(panel_handle, NULL);

    // uint16_t row_line = BSP_LCD_V_RES / 16;
    // uint16_t *color = (uint16_t *)malloc(row_line * BSP_LCD_H_RES * 2);
    // for (int j = 0; j < 16; j++) {
    //     for (int i = 0; i < row_line * BSP_LCD_H_RES; i++) {
    //         color[i] = 0x0001 << j;
    //     }
    //     esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, BSP_LCD_H_RES, (j + 1) * row_line, color);
    // }
    // for (;;) {
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    // }

    // alloc draw buffers used by LVGL
    void *buf1 = NULL;
    void *buf2 = NULL;
    int buffer_size;
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
    buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES;
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH && CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 3
    BSP_ERROR_CHECK_RETURN_NULL(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 3, &lvgl_port_rgb_last_buf, &buf1, &buf2));
    lvgl_port_rgb_next_buf = lvgl_port_rgb_last_buf;
    lvgl_port_flush_next_buf = buf2;
#else
    BSP_ERROR_CHECK_RETURN_NULL(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
#endif
#else
    buffer_size = BSP_LCD_H_RES * LVGL_BUFFER_HEIGHT;
    buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), LVGL_BUFFER_MALLOC);
    BSP_NULL_CHECK(buf1, NULL);
#endif
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BSP_LCD_H_RES;
    disp_drv.ver_res = BSP_LCD_V_RES;
    disp_drv.flush_cb = lvgl_port_flush_callback;
    disp_drv.drv_update_cb = lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
    disp_drv.full_refresh = 1;
#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
    disp_drv.direct_mode = 1;
#endif
    return lv_disp_drv_register(&disp_drv);
}

#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
static bool lvgl_port_lcd_trans_done(esp_lcd_panel_handle_t handle)
{
    BaseType_t need_yield = pdFALSE;
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH && CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 3
    if (lvgl_port_rgb_next_buf != lvgl_port_rgb_last_buf) {
        lvgl_port_flush_next_buf = lvgl_port_rgb_last_buf;
        lvgl_port_rgb_last_buf = lvgl_port_rgb_next_buf;
    }
#else
    xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);
#endif
    return (need_yield == pdTRUE);
}
#endif

static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_v = 0;

    assert(indev_drv);
    assert(indev_drv->user_data);
    knob_handle_t knob_handle = (knob_handle_t)indev_drv->user_data;

    int32_t invd = iot_knob_get_count_value(knob_handle);
    knob_event_t event = iot_knob_get_event(knob_handle);

    if (last_v ^ invd) {
        last_v = invd;
        data->enc_diff = (KNOB_RIGHT == event) ? (-1) : ((KNOB_LEFT == event) ? (1) : (0));
    } else {
        data->enc_diff = 0;
    }
    data->state = (true == bsp_button_get(BSP_BTN_PRESS)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static esp_err_t lvgl_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_t *indev_encoder;
    knob_handle_t knob_handle;

    BSP_ERROR_CHECK_RETURN_NULL(bsp_button_init(BSP_BTN_PRESS));

    knob_config_t *cfg = calloc(1, sizeof(knob_config_t));
    cfg->default_direction = 0;
    cfg->gpio_encoder_a = BSP_ENCODER_A;
    cfg->gpio_encoder_b = BSP_ENCODER_B;
    knob_handle = iot_knob_create(cfg);

    /* Register a touchpad input device */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read;
    indev_drv.user_data = knob_handle;
    indev_encoder = lv_indev_drv_register(&indev_drv);
    BSP_NULL_CHECK(indev_encoder, ESP_ERR_NO_MEM);

    return ESP_OK;
}

lv_disp_t *bsp_display_start(void)
{
    lv_init();
    lv_disp_t *disp = lvgl_port_display_init();
    BSP_NULL_CHECK(disp, NULL);
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_tick_init());
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_indev_init());

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    BSP_NULL_CHECK(lvgl_mux, NULL);
    xTaskCreate(
        lvgl_port_task, "LVGL task", CONFIG_BSP_DISPLAY_LVGL_TASK_STACK_SIZE * 1024, NULL,
        CONFIG_BSP_DISPLAY_LVGL_TASK_PRIORITY, &lvgl_task_handle
    );
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
    bsp_lcd_register_trans_done_callback(lvgl_port_lcd_trans_done);
#endif

    return disp;
}

void bsp_display_rotate(lv_disp_t *disp, lv_disp_rot_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void bsp_display_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

/**************************************************************************************************
 *
 * Button Funciton
 *
 **************************************************************************************************/
esp_err_t bsp_button_init(const bsp_button_t btn)
{
    const gpio_config_t button_io_config = {
        .pin_bit_mask = BIT64(btn),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    return gpio_config(&button_io_config);
}

bool bsp_button_get(const bsp_button_t btn)
{
    return !(bool)gpio_get_level(btn);
}
