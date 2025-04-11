#include "HAL.hpp"

#include <WiFi.h>
#include <lvgl.h>

#include "driver.h"

static const char* TAG = "HAL";

LGFX gfx;

tm HAL::getTime() {
    tm time;
    time.tm_min = 37;
    time.tm_hour = 13;
    return time;
}

void HAL::init(bool synchronousStart) {
    lv_init();

    gfx.begin();
    gfx.setBrightness(255);
    gfx.setRotation(1);

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
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.user_data = this;
    indev_drv.read_cb = [](struct _lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
        static_cast<HAL*>(indev_drv->user_data)->readTouchScreen(indev_drv, data);
    };
    lv_indev_drv_register(&indev_drv);
}

void HAL::flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (this->displayFlushState == 0) {
        gfx.startWrite();
        this->displayFlushState = 1;
    }

    gfx.pushImage(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (lgfx::rgb565_t*)&color_p->full);
    if (lv_disp_flush_is_last(disp)) {
        gfx.endWrite();
        this->displayFlushState = 0;
    }

    lv_disp_flush_ready(disp);
}

void HAL::readTouchScreen(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_REL;

    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY)) {
        lv_disp_enable_invalidation(NULL, true);

        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;

        TaskHandle_t taskHandle = xTaskGetHandle("pwrMngmntTask");
        if (taskHandle != nullptr) {
            xTaskNotify(taskHandle, 2, eSetValueWithOverwrite);
        }
    }
}

void HAL::startSleep() {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}