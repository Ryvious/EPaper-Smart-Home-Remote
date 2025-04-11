#include "Hal.hpp"

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <lvgl.h>

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/adc_channel.h"

static const char* TAG = "HAL";

int32_t HAL::getBatteryLevel() {
    auto level = M5.Power.getBatteryLevel();
    ESP_LOGV(TAG, "Battery level: %u", level);

    return level;
}

tm HAL::getTime() {
    return M5.Rtc.getDateTime().get_tm();
}

void HAL::init(bool synchronousStart) {
    m5::M5Unified::config_t cfg;
    cfg.clear_display = !synchronousStart;
    M5.begin(cfg);

    lv_init();
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
        this->displayFlushState = 1;
        M5.Display.startWrite();
    }

    for (auto y = area->y1; y <= area->y2; y++) {
        for (auto x = area->x1; x <= area->x2; x++) {
            auto col = lv_color_to16(*color_p);
            M5.Display.drawPixel(x, y, col);
            color_p++;
        }
    }

    if (lv_disp_flush_is_last(disp)) {
        M5.Display.endWrite();
        this->displayFlushState = 0;
        this->displayReady = true;
    }

    lv_disp_flush_ready(disp);
}

void HAL::readTouchScreen(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    M5.update();
    auto count = M5.Touch.getCount();
    if (count == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        auto touch = M5.Touch.getDetail(0);
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.x;
        data->point.y = touch.y;

        TaskHandle_t taskHandle = xTaskGetHandle("pwrMngmntTask");
        if (taskHandle != nullptr) {
            xTaskNotify(taskHandle, 2, eSetValueWithOverwrite);
        }
    }
}

void HAL::startSleep() {
    M5.Power.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR, true);
}