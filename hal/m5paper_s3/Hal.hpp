#pragma once
#ifndef BOARD_HAS_PSRAM
    #error "Please enable PSRAM !!!"
#endif

#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_adc_cal.h>
#include <lvgl.h>

#include "IHal.hpp"

#define SCREENHEIGHT 960
#define SCREENWIDTH 540

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 60

class HAL : public IHalInterface {
   private:
    lv_color_t displayBuffer[SCREENWIDTH * 10];
    lv_disp_draw_buf_t displayDrawBuffer;
    uint16_t displayFlushState = 0;
    boolean displayReady = false;

   public:
    void init(bool synchronousStart) override;

    int32_t getBatteryLevel() override;
    tm getTime() override;
    bool isDisplayReady() override { return this->displayReady; }
    bool isNetworkConnected() override { return WiFi.status() == WL_CONNECTED; }
    void startSleep() override;

   protected:
    void flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    void readTouchScreen(lv_indev_drv_t* drv, lv_indev_data_t* data);
};