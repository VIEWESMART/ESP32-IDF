/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"

#include "sdkconfig.h"
#include "../priv_include/bsp_err_check.h"
#include "../priv_include/bsp_sub_board.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "bsp_lcd";

static bsp_lcd_trans_done_cb_t trans_done = NULL;
#if CONFIG_BSP_LCD_RGB_REFRESH_TASK_ENABLE
static TaskHandle_t lcd_task_handle = NULL;
#endif

static esp_err_t lcd_config(void);

/**************************************************************************************************
 *
 * LCD Panel Function
 *
 **************************************************************************************************/
IRAM_ATTR static bool on_vsync_event(esp_lcd_panel_handle_t panel, esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
#if CONFIG_BSP_LCD_RGB_REFRESH_MANUALLY
    xTaskNotifyFromISR(lcd_task_handle, ULONG_MAX, eNoAction, &need_yield);
#endif
    if (trans_done) {
        if (trans_done(panel)) {
            need_yield = pdTRUE;
        }
    }

    return (need_yield == pdTRUE);
}

#if CONFIG_BSP_LCD_RGB_REFRESH_MANUALLY
static void lcd_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LCD refresh task");

    TickType_t tick;
    for (;;) {
        esp_lcd_rgb_panel_refresh((esp_lcd_panel_handle_t)arg);
        tick = xTaskGetTickCount();
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(CONFIG_BSP_LCD_RGB_REFRESH_TASK_PERIOD));
    }
}
#endif

esp_lcd_panel_handle_t bsp_lcd_init(void *arg)
{
    BSP_ERROR_CHECK_RETURN_ERR(lcd_config());

    ESP_LOGI(TAG, "Initialize RGB panel");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .psram_trans_align = 64,
        .data_width = 16,
        .de_gpio_num = BSP_LCD_DE,
        .pclk_gpio_num = BSP_LCD_PCLK,
        .vsync_gpio_num = BSP_LCD_VSYNC,
        .hsync_gpio_num = BSP_LCD_HSYNC,
        .data_gpio_nums = {
            BSP_LCD_DATA0,
            BSP_LCD_DATA1,
            BSP_LCD_DATA2,
            BSP_LCD_DATA3,
            BSP_LCD_DATA4,
            BSP_LCD_DATA5,
            BSP_LCD_DATA6,
            BSP_LCD_DATA7,
            BSP_LCD_DATA8,
            BSP_LCD_DATA9,
            BSP_LCD_DATA10,
            BSP_LCD_DATA11,
            BSP_LCD_DATA12,
            BSP_LCD_DATA13,
            BSP_LCD_DATA14,
            BSP_LCD_DATA15,
        },
        .timings = {
            .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .hsync_back_porch = BSP_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = BSP_LCD_HSYNC_FRONT_PORCH,
            .hsync_pulse_width = BSP_LCD_HSYNC_PULSE_WIDTH,
            .vsync_back_porch = BSP_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = BSP_LCD_VSYNC_FRONT_PORCH,
            .vsync_pulse_width = BSP_LCD_VSYNC_PULSE_WIDTH,
            .flags.pclk_active_neg = BSP_LCD_PCLK_ACTIVE_NEG,
        },
        .flags.fb_in_psram = 1,
#if CONFIG_BSP_LCD_RGB_REFRESH_MANUALLY
        .flags.refresh_on_demand = 1,
#endif
#if CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 2
        .flags.double_fb = 1,
#elif CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 3
        .num_fbs = 3,
#endif
#if CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_MODE
        .bounce_buffer_size_px = BSP_LCD_H_RES * CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT,
#endif
    };
    BSP_ERROR_CHECK_RETURN_NULL(esp_lcd_new_rgb_panel(&panel_conf, &panel_handle));
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = on_vsync_event,
    };
    esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL);
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

#if CONFIG_BSP_LCD_RGB_REFRESH_MANUALLY
    xTaskCreate(lcd_task, "LCD task", 2048, panel_handle, CONFIG_BSP_LCD_RGB_REFRESH_TASK_PRIORITY, &lcd_task_handle);
#endif

    return panel_handle;
}

esp_err_t bsp_lcd_register_trans_done_callback(bsp_lcd_trans_done_cb_t callback)
{
#if CONFIG_LCD_RGB_ISR_IRAM_SAFE
    if (callback) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(callback), ESP_ERR_INVALID_ARG, TAG, "Callback not in IRAM");
    }
#endif
    trans_done = callback;

    return ESP_OK;
}

/**************************************************************************************************
 *
 * LCD Configuration Function
 *
 **************************************************************************************************/
#define Delay(t)        vTaskDelay(pdMS_TO_TICKS(t))
#define udelay(t)       esp_rom_delay_us(t)
#define CS(n)           BSP_ERROR_CHECK_RETURN_ERR(gpio_set_level(BSP_LCD_SPI_CS, n))
#define SCK(n)          BSP_ERROR_CHECK_RETURN_ERR(gpio_set_level(BSP_LCD_SPI_SCK, n))
#define SDO(n)          BSP_ERROR_CHECK_RETURN_ERR(gpio_set_level(BSP_LCD_SPI_SDO, n))

/**
 * @brief Simulate SPI to write data using io expander
 *
 * @param data: Data to write
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
static esp_err_t spi_write(uint16_t data)
{
    for (uint8_t n = 0; n < 9; n++) {
        if (data & 0x0100) {
            SDO(1);
        } else {
            SDO(0);
        }
        data = data << 1;

        SCK(0);
        udelay(10);
        SCK(1);
        udelay(10);
    }

    return ESP_OK;
}

/**
 * @brief Simulate SPI to write LCD command using io expander
 *
 * @param data: LCD command to write
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
static esp_err_t spi_write_cmd(uint16_t data)
{
    CS(0);
    udelay(10);

    spi_write((data & 0x00FF));

    udelay(10);
    CS(1);
    SCK(1);
    SDO(1);
    udelay(10);

    return ESP_OK;
}

/**
 * @brief Simulate SPI to write LCD data using io expander
 *
 * @param data: LCD data to write
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
static esp_err_t spi_write_data(uint16_t data)
{
    CS(0);
    udelay(10);

    data &= 0x00FF;
    data |= 0x0100;
    spi_write(data);

    udelay(10);
    CS(1);
    SCK(1);
    SDO(1);
    udelay(10);

    return ESP_OK;
}

/**
 * @brief LCD configuration data structure type
 *
 */
typedef struct {
    uint8_t cmd;            // LCD command
    uint8_t data[52];       // LCD data
    uint8_t data_bytes;     // Length of data in above data array; 0xFF = end of cmds.
} lcd_config_data_t;

const static lcd_config_data_t LCD_CONFIG_CMD[] = {
    {0xF0, {0x55, 0xAA, 0x52, 0x08, 0x00}, 5},
    {0xF6, {0x5A, 0x87}, 2},
    {0xC1, {0x3F}, 1},
    {0xCD, {0x25}, 1},
    {0xC9, {0x10}, 1},
    {0xF8, {0x8A}, 1},
    {0xAC, {0x45}, 1},
    {0xA7, {0x47}, 1},
    {0xA0, {0xDD}, 1},//0xFF 20200413 James
    {0x87, {0x04, 0x03, 0x66}, 3},
    {0x86, {0x99, 0xa3, 0xa3, 0x51}, 4},//0X71 20200413 James
    {0xFA, {0x08, 0x08, 0x08, 0x04}, 4},
    //-----for vcom-----//
    {0x9A, {0x8a}, 1},  //4A
    {0x9B, {0x62}, 1},   //22
    {0x82, {0x48, 0x48}, 2},  //08-08
    //-----for vcom-----//
    {0xB1, {0x10}, 1},
    {0x7A, {0x13, 0x1A}, 2},
    {0x7B, {0x13, 0x1A}, 2},
    {0x6D, {0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x02, 0x0b, 0x01, 0x00, 0x1f, 0x1e, 0x09, 0x0f, 0x1e, 0x1e, 0x1e, 0x1e,
            0x10, 0x0a, 0x1e, 0x1f, 0x00, 0x08, 0x0b, 0x02, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e}, 32},
    {0x64, {0x18, 0x07, 0x01, 0xE7, 0x03, 0x03, 0x18, 0x06, 0x01, 0xE6, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16},
    {0x65, {0x58, 0x26, 0x18, 0x2c, 0x03, 0x03, 0x58, 0x26, 0x18, 0x2c, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16},
    {0x66, {0x58, 0x26, 0x18, 0x2c, 0x03, 0x03, 0x58, 0x26, 0x18, 0x2c, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16},
    {0x67, {0x18, 0x05, 0x01, 0xE5, 0x03, 0x03, 0x18, 0x04, 0x01, 0xE4, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16},
    {0x60, {0x18, 0x09, 0x7A, 0x7A, 0x51, 0xF1, 0x7A, 0x7A}, 8},
    {0x63, {0x51, 0xF1, 0x7A, 0x7A, 0x18, 0x08, 0x7A, 0x7A}, 8},
    {0xD1, {0x00, 0x00, 0x00, 0x0E, 0x00, 0x31, 0x00, 0x4E, 0x00, 0x67, 0x00, 0x92, 0x00, 0xB5, 0x00, 0xED, 0x01, 0x1C,
            0x01, 0x66, 0x01, 0xA4, 0x02, 0x04, 0x02, 0x53, 0x02, 0x56, 0x02, 0x9F, 0x02, 0xF3, 0x03, 0x29, 0x03, 0x73,
            0x03, 0xA1, 0x03, 0xB9, 0x03, 0xC8, 0x03, 0xDB, 0x03, 0xE7, 0x03, 0xF4, 0x03, 0xFB, 0x03, 0XFF}, 52},
    {0xD2, {0x00, 0x00, 0x00, 0x0E, 0x00, 0x31, 0x00, 0x4E, 0x00, 0x67, 0x00, 0x92, 0x00, 0xB5, 0x00, 0xED, 0x01, 0x1C,
            0x01, 0x66, 0x01, 0xA4, 0x02, 0x04, 0x02, 0x53, 0x02, 0x56, 0x02, 0x9F, 0x02, 0xF3, 0x03, 0x29, 0x03, 0x73,
            0x03, 0xA1, 0x03, 0xB9, 0x03, 0xC8, 0x03, 0xDB, 0x03, 0xE7, 0x03, 0xF4, 0x03, 0xFB, 0x03, 0XFF}, 52},
    {0xD3, {0x00, 0x00, 0x00, 0x0E, 0x00, 0x31, 0x00, 0x4E, 0x00, 0x67, 0x00, 0x92, 0x00, 0xB5, 0x00, 0xED, 0x01, 0x1C,
            0x01, 0x66, 0x01, 0xA4, 0x02, 0x04, 0x02, 0x53, 0x02, 0x56, 0x02, 0x9F, 0x02, 0xF3, 0x03, 0x29, 0x03, 0x73,
            0x03, 0xA1, 0x03, 0xB9, 0x03, 0xC8, 0x03, 0xDB, 0x03, 0xE7, 0x03, 0xF4, 0x03, 0xFB, 0x03, 0XFF}, 52},
    {0xD4, {0x00, 0x00, 0x00, 0x0E, 0x00, 0x31, 0x00, 0x4E, 0x00, 0x67, 0x00, 0x92, 0x00, 0xB5, 0x00, 0xED, 0x01, 0x1C,
            0x01, 0x66, 0x01, 0xA4, 0x02, 0x04, 0x02, 0x53, 0x02, 0x56, 0x02, 0x9F, 0x02, 0xF3, 0x03, 0x29, 0x03, 0x73,
            0x03, 0xA1, 0x03, 0xB9, 0x03, 0xC8, 0x03, 0xDB, 0x03, 0xE7, 0x03, 0xF4, 0x03, 0xFB, 0x03, 0XFF}, 52},
    {0xD5, {0x00, 0x00, 0x00, 0x0E, 0x00, 0x31, 0x00, 0x4E, 0x00, 0x67, 0x00, 0x92, 0x00, 0xB5, 0x00, 0xED, 0x01, 0x1C,
            0x01, 0x66, 0x01, 0xA4, 0x02, 0x04, 0x02, 0x53, 0x02, 0x56, 0x02, 0x9F, 0x02, 0xF3, 0x03, 0x29, 0x03, 0x73,
            0x03, 0xA1, 0x03, 0xB9, 0x03, 0xC8, 0x03, 0xDB, 0x03, 0xE7, 0x03, 0xF4, 0x03, 0xFB, 0x03, 0XFF}, 52},
    {0xD6, {0x00, 0x00, 0x00, 0x0E, 0x00, 0x31, 0x00, 0x4E, 0x00, 0x67, 0x00, 0x92, 0x00, 0xB5, 0x00, 0xED, 0x01, 0x1C,
            0x01, 0x66, 0x01, 0xA4, 0x02, 0x04, 0x02, 0x53, 0x02, 0x56, 0x02, 0x9F, 0x02, 0xF3, 0x03, 0x29, 0x03, 0x73,
            0x03, 0xA1, 0x03, 0xB9, 0x03, 0xC8, 0x03, 0xDB, 0x03, 0xE7, 0x03, 0xF4, 0x03, 0xFB, 0x03, 0XFF}, 52},
    {0x11, {0x00}, 0},
    {0x00, {0x00}, 0xff},
};

/**
 * @brief Configure LCD with specific commands and data
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 *
 */
static esp_err_t lcd_config(void)
{
    gpio_config_t config = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pin_bit_mask = BIT64(BSP_LCD_SPI_CS) | BIT64(BSP_LCD_SPI_SCK) | BIT64(BSP_LCD_SPI_SDO) | BIT64(BSP_LCD_RST) | BIT64(BSP_LCD_BL),
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "SPI GPIO config failed");
    gpio_set_level(BSP_LCD_RST, 1);
    CS(1);
    SCK(1);
    SDO(1);

    uint8_t i = 0;
    while (LCD_CONFIG_CMD[i].data_bytes != 0xff) {
        BSP_ERROR_CHECK_RETURN_ERR(spi_write_cmd(LCD_CONFIG_CMD[i].cmd));
        for (uint8_t j = 0; j < LCD_CONFIG_CMD[i].data_bytes; j++) {
            BSP_ERROR_CHECK_RETURN_ERR(spi_write_data(LCD_CONFIG_CMD[i].data[j]));
        }
        i++;
    }
    vTaskDelay(pdMS_TO_TICKS(120));
    BSP_ERROR_CHECK_RETURN_ERR(spi_write_cmd(0x29));
    vTaskDelay(pdMS_TO_TICKS(20));

    // gpio_reset_pin(BSP_LCD_SPI_CS);
    gpio_reset_pin(BSP_LCD_SPI_SCK);
    gpio_reset_pin(BSP_LCD_SPI_SDO);

    return ESP_OK;
}
