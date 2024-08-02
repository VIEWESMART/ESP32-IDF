/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "lvgl.h"

#include "sdkconfig.h"

/**************************************************************************************************
 *  BSP Pinout
 **************************************************************************************************/
/* Display */
#define BSP_LCD_VSYNC           (GPIO_NUM_3)
#define BSP_LCD_HSYNC           (GPIO_NUM_46)
#define BSP_LCD_DE              (GPIO_NUM_17)
#define BSP_LCD_RST             (GPIO_NUM_8)
#define BSP_LCD_PCLK            (GPIO_NUM_9)
#define BSP_LCD_DATA0           (GPIO_NUM_10)   // B3
#define BSP_LCD_DATA1           (GPIO_NUM_11)   // B4
#define BSP_LCD_DATA2           (GPIO_NUM_12)   // B5
#define BSP_LCD_DATA3           (GPIO_NUM_13)   // B6
#define BSP_LCD_DATA4           (GPIO_NUM_14)   // B7
#define BSP_LCD_DATA5           (GPIO_NUM_21)   // G2
#define BSP_LCD_DATA6           (GPIO_NUM_47)   // G3
#define BSP_LCD_DATA7           (GPIO_NUM_48)   // G4
#define BSP_LCD_DATA8           (GPIO_NUM_45)   // G5
#define BSP_LCD_DATA9           (GPIO_NUM_38)   // G6
#define BSP_LCD_DATA10          (GPIO_NUM_39)   // G7
#define BSP_LCD_DATA11          (GPIO_NUM_40)   // R3
#define BSP_LCD_DATA12          (GPIO_NUM_41)   // R4
#define BSP_LCD_DATA13          (GPIO_NUM_42)   // R5
#define BSP_LCD_DATA14          (GPIO_NUM_2)    // R6
#define BSP_LCD_DATA15          (GPIO_NUM_1)    // R7
#define BSP_LCD_SPI_CS          (GPIO_NUM_18)
#define BSP_LCD_SPI_SCK         (GPIO_NUM_13)
#define BSP_LCD_SPI_SDO         (GPIO_NUM_12)

#define BSP_LCD_BL              (GPIO_NUM_7)

/* Buttons */
typedef enum {
    BSP_BTN_PRESS = GPIO_NUM_0,
} bsp_button_t;

#define BSP_ENCODER_A         (GPIO_NUM_6)
#define BSP_ENCODER_B         (GPIO_NUM_5)

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************************************************************************
 *
 * Display Interface
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling `bsp_display_lock()` before calling any LVGL API (lv_...) and then give the mutex with
 * `bsp_display_unlock()`.
 *
 *******************************************************************************************************************************/
/* LCD related parameters */
#define BSP_LCD_H_RES                   (480)
#define BSP_LCD_V_RES                   (480)
#define BSP_LCD_PIXEL_CLOCK_HZ          (26 * 1000 * 1000)
#define BSP_LCD_HSYNC_BACK_PORCH        (20)
#define BSP_LCD_HSYNC_FRONT_PORCH       (40)
#define BSP_LCD_HSYNC_PULSE_WIDTH       (8)
#define BSP_LCD_VSYNC_BACK_PORCH        (20)
#define BSP_LCD_VSYNC_FRONT_PORCH       (50)
#define BSP_LCD_VSYNC_PULSE_WIDTH       (8)
#define BSP_LCD_PCLK_ACTIVE_NEG         (false)

/* LVGL related parameters */
#define LVGL_TICK_PERIOD_MS         (CONFIG_BSP_DISPLAY_LVGL_TICK)
#define LVGL_BUFFER_HEIGHT          (CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT)
#if CONFIG_BSP_DISPLAY_LVGL_PSRAM
#define LVGL_BUFFER_MALLOC          (MALLOC_CAP_SPIRAM)
#else
#define LVGL_BUFFER_MALLOC          (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif

/**
 * @brief Initialize display
 *
 * @note This function initializes display controller and starts LVGL handling task.
 * @note Users can get LCD panel handle from `user_data` in returned display.
 *
 * @return Pointer to LVGL display or NULL when error occured
 */
lv_disp_t *bsp_display_start(void);

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate screen
 *
 * @note  Display must be already initialized by calling `bsp_display_start()`
 *
 * @param[in] disp:     Pointer to LVGL display
 * @param[in] rotation: Angle of the display rotation
 */
void bsp_display_rotate(lv_disp_t *disp, lv_disp_rot_t rotation);

/**************************************************************************************************
 *
 * Button
 *
 * There are 2 buttons on ESP32_C3_LCD_EV_BOARD:
 *  - Reset:  Not programable
 *  - Config: Controls boot mode during reset. Can be programmed after application starts
 **************************************************************************************************/
/**
 * @brief Set button's GPIO as input
 *
 * @param[in] btn Button to be initialized
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t bsp_button_init(const bsp_button_t btn);

/**
 * @brief Get button's state
 *
 * @param[in] btn Button to read
 * @return true  Button pressed
 * @return false Button released
 */
bool bsp_button_get(const bsp_button_t btn);

#ifdef __cplusplus
}
#endif
