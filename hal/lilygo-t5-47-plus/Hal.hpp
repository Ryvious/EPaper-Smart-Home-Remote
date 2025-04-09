#pragma once
#include <WiFi.h>
#include <lvgl.h>
#include <touch.h>

#include "IHal.hpp"
#include "epd_driver.h"

#define SCREENHEIGHT EPD_WIDTH  // swapped due to rotation
#define SCREENWIDTH EPD_HEIGHT  // swapped due to rotation

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 60

class HAL : public IHalInterface {
   private:
    lv_color_t displayBuffer[SCREENWIDTH * 10];
    lv_disp_draw_buf_t displayDrawBuffer;
    lv_area_t displayFlushArea;
    uint8_t* displayFlushBuffer;
    uint16_t displayFlushState = 0;
    boolean displayReady = false;

   public:
    void init(bool synchronousStart) override;

    tm getTime() override;
    bool isDisplayReady() override { return this->displayReady; }
    bool isNetworkConnected() override { return WiFi.status() == WL_CONNECTED; }
    void startSleep() override;

   protected:
    void flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    void readTouchScreen(lv_indev_drv_t* drv, lv_indev_data_t* data);
};