#include "Hal.hpp"

#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <touch.h>

#include "epd_driver.h"
#include "pins.h"

TouchClass touch;

void delayedTask(void* param) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    vTaskDelete(NULL);
}

tm HAL::getTime() {
    tm time;
    time.tm_min = 37;
    time.tm_hour = 13;
    return time;
}

void HAL::init(bool synchronousStart) {
    lv_init();

    pinMode(TOUCH_INT, INPUT_PULLUP);
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    if (!touch.begin()) {
        Serial.println("start touchscreen failed");
        while (1);
    }

    epd_init();

    lv_disp_draw_buf_init(&this->displayDrawBuffer, this->displayBuffer, NULL, SCREENWIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.user_data = this;
    disp_drv.hor_res = SCREENWIDTH;
    disp_drv.ver_res = SCREENHEIGHT;
    disp_drv.flush_cb = [](struct _lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
        static_cast<HAL*>(disp_drv->user_data)->flushDisplay(disp_drv, area, color_p);
    };
    disp_drv.draw_buf = &this->displayDrawBuffer;
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_90;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.user_data = this;
    indev_drv.read_cb = [](struct _lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
        static_cast<HAL*>(indev_drv->user_data)->readTouchScreen(indev_drv, data);
    };
    lv_indev_drv_register(&indev_drv);

    this->displayFlushBuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), SCREENHEIGHT * SCREENWIDTH / 2);
    if (!displayFlushBuffer) {
        Serial.println("alloc memory failed !!!");
        while (1);
    }
    memset(displayFlushBuffer, 0xFF, SCREENHEIGHT * SCREENWIDTH / 2);

    epd_poweron();
    epd_clear();
    // epd_poweroff();
}

void HAL::flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (this->displayFlushState == 0) {
        lv_area_copy(&this->displayFlushArea, area);
        this->displayFlushState = 1;
    } else {
        if (area->x1 < this->displayFlushArea.x1) {
            this->displayFlushArea.x1 = area->x1;
        }
        if (area->x2 > this->displayFlushArea.x2) {
            this->displayFlushArea.x2 = area->x2;
        }
        if (area->y1 < this->displayFlushArea.y1) {
            this->displayFlushArea.y1 = area->y1;
        }
        if (area->y2 > this->displayFlushArea.y2) {
            this->displayFlushArea.y2 = area->y2;
        }
    }

    int32_t x, y;
    for (y = area->y1; y <= area->y2; y++) {
        for (x = area->x1; x <= area->x2; x++) {
            uint8_t col = lv_color_brightness(*color_p);
            epd_draw_pixel(x, y, col, this->displayFlushBuffer);
            color_p++;
        }
    }

    if (lv_disp_flush_is_last(disp)) {
        // const uint32_t w = this->displayFlushArea.x2 - this->displayFlushArea.x1 + 1;
        // const uint32_t h = this->displayFlushArea.y2 - this->displayFlushArea.y1 + 1;
        // const Rect_t rect = {.x = this->displayFlushArea.x1, .y = this->displayFlushArea.y1, .width = w, .height = h};

        // epd_poweron();
        // epd_clear_area_cycles(area_draw, 1, 10);
        epd_clear_area_cycles(epd_full_screen(), 1, 10);
        epd_draw_grayscale_image(epd_full_screen(), this->displayFlushBuffer);
        // epd_poweroff();

        this->displayFlushState = 0;

        // TaskHandle_t taskHandle = xTaskGetHandle("DelayedTask");
        // if (taskHandle != NULL) {
        //     vTaskDelete(taskHandle);
        // }
        // xTaskCreate(delayedTask, "DelayedTask", 2048, NULL, 5, NULL);

        this->displayReady = true;
    }

    lv_disp_flush_ready(disp);
}

void HAL::readTouchScreen(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_REL;
    
    if (touch.scanPoint()) {
        uint16_t touchX, touchY;
        touch.getPoint(touchX, touchY, 0);
        data->state = LV_INDEV_STATE_PR;
        data->point.y = 525 - touchY;   // "invert" due to rotation
        data->point.x = touchX;
    }
}

void HAL::startSleep() {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}