/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef LV_EXAMPLE_IMAGE_H
#define LV_EXAMPLE_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
LV_IMG_DECLARE(icon_washing);
LV_IMG_DECLARE(icon_thermostat);
LV_IMG_DECLARE(icon_light);
LV_IMG_DECLARE(icon_washing_ns);
LV_IMG_DECLARE(icon_thermostat_ns);
LV_IMG_DECLARE(icon_light_ns);

// LV_IMG_DECLARE(espressif_logo);
LV_IMG_DECLARE(imglogo);

LV_IMG_DECLARE(light_close_bg)
LV_IMG_DECLARE(light_close_pwm)
LV_IMG_DECLARE(light_close_status)
LV_IMG_DECLARE(light_cool_100)
LV_IMG_DECLARE(light_cool_25)
LV_IMG_DECLARE(light_cool_50)
LV_IMG_DECLARE(light_cool_75)
LV_IMG_DECLARE(light_cool_bg)
LV_IMG_DECLARE(light_warm_100)
LV_IMG_DECLARE(light_warm_25)
LV_IMG_DECLARE(light_warm_50)
LV_IMG_DECLARE(light_warm_75)
LV_IMG_DECLARE(light_warm_bg)

LV_IMG_DECLARE(light_pwm_00)
LV_IMG_DECLARE(light_pwm_25)
LV_IMG_DECLARE(light_pwm_50)
LV_IMG_DECLARE(light_pwm_75)
LV_IMG_DECLARE(light_pwm_100)

LV_IMG_DECLARE(img_washing_bg);
LV_IMG_DECLARE(img_washing_cut_bg);
LV_IMG_DECLARE(img_washing_wave1);
LV_IMG_DECLARE(img_washing_wave2);
LV_IMG_DECLARE(img_washing_bubble1);
LV_IMG_DECLARE(img_washing_bubble2);

/**
 * @brief Construct a new lv img declare object
 * clother
 */
LV_IMG_DECLARE(wash_underwear1)
LV_IMG_DECLARE(wash_underwear2)
LV_IMG_DECLARE(wash_shirt)

/**
 * @brief Construct a new lv img declare object
 * Name
 */
LV_IMG_DECLARE(img_washing_stand);
LV_IMG_DECLARE(img_washing_shirt);
LV_IMG_DECLARE(img_washing_underwear);
LV_IMG_DECLARE(wash_basic)
LV_IMG_DECLARE(wash_blouse)
LV_IMG_DECLARE(wash_briefs)

LV_IMG_DECLARE(AC_BG)
LV_IMG_DECLARE(AC_unit)

LV_IMG_DECLARE(standby_eye_left)
LV_IMG_DECLARE(standby_eye_right)
LV_IMG_DECLARE(standby_eye_1)
LV_IMG_DECLARE(standby_eye_2)
LV_IMG_DECLARE(standby_eye_close)
LV_IMG_DECLARE(standby_face)
LV_IMG_DECLARE(standby_mouth_2)
LV_IMG_DECLARE(standby_eye_1_fade)
LV_IMG_DECLARE(standby_eye_3)
LV_IMG_DECLARE(standby_eye_open)
LV_IMG_DECLARE(standby_mouth_1)

LV_IMG_DECLARE(language_bg)
LV_IMG_DECLARE(language_bg_dither)
LV_IMG_DECLARE(language_select)
LV_IMG_DECLARE(language_unselect)

/********************************
 * font
********************************/
/*0x20-0x7F*/
LV_FONT_DECLARE(HelveticaNeue_Regular_40);

/*0123456789：-%*/
LV_FONT_DECLARE(HelveticaNeue_Regular_48);

/*0123456789:-*/
LV_FONT_DECLARE(HelveticaNeue_Regular_80);

/*0x20-0x7F*/
/*
-%:标准洗内衣洗衬衣长按结束洗衣模式恒温器照明模式红外背光喇叭编码器成功失败左右按下
屏幕正常亮暗下一步左右旋按下屏幕正常亮暗下一步跳帽先接后再等待结果
*/
LV_FONT_DECLARE(SourceHanSansCN_Normal30);

/*
洗衣模式洗衣模式恒温器照明中文已恢复出厂设置
*/
LV_FONT_DECLARE(font_SourceHanSansCN_38);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EXAMPLE_IMAGE_H*/