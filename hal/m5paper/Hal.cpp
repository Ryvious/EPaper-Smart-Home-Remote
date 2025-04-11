#include "Hal.hpp"

#include <WiFi.h>
#include <esp_wifi.h>
#include <lvgl.h>

#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/adc_channel.h"

static const char* TAG = "HAL";

LGFX gfx;
RtcPCF8563<TwoWire> rtc(Wire);

void delayedTask(void* param) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "DelayedTask");
    lv_obj_invalidate(lv_scr_act());
    vTaskDelete(NULL);
}

// https://docs.espressif.com/projects/esp-idf/en/release-v3.3/api-reference/peripherals/adc.html
void HAL::beginBatteryADC() {
    if (this->adcInitialized) {
        return;
    }

    adc_power_acquire();
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_BAT, ADC_ATTEN_DB_11);
    this->adcCharacteristics = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, BAT_BASE_VOLTAGE, this->adcCharacteristics);

    this->adcInitialized = true;
}

int32_t HAL::getBatteryLevel() {
    uint32_t voltage = constrain(this->getBatteryVoltage(), 3300, 4200);
    float percentage = (float)(voltage - 3300) / (float)(4200 - 3300);

    return (int)(constrain(percentage, 0.01, 1) * 100);
}

int16_t HAL::getBatteryVoltage() {
    uint32_t adc_raw_value = 0;
    for (uint_fast16_t i = 0; i < ADC_FILTER_SAMPLE; i++) {
        adc_raw_value += adc1_get_raw(ADC_CHANNEL_BAT);
    }

    adc_raw_value = adc_raw_value / ADC_FILTER_SAMPLE;
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_raw_value, this->adcCharacteristics);
    voltage = static_cast<uint32_t>(voltage * BAT_ADC_SCALE);
    ESP_LOGV(TAG, "Battery voltage: %u", voltage);

    return voltage;
}

tm HAL::getTime() {
    RtcDateTime now = rtc.GetDateTime();
    tm time;
    time.tm_min = (int)now.Minute();
    time.tm_hour = (int)now.Hour();
    return time;
}

void HAL::init(bool synchronousStart) {
    this->displaySemaphore = xSemaphoreCreateMutex();
    this->displayReady = false;
    this->beginBatteryADC();

    rtc.Begin();

    ESP_LOGI("BAT", "%d", this->getBatteryVoltage());
    lv_init();
    if (synchronousStart) {
        gfx.wakeup();
        gfx.init_without_reset();
    } else {
        gfx.begin();
    }
    gfx.setEpdMode(epd_mode_t::epd_quality);
    gfx.setRotation(0);
    gfx.setBrightness(128);

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

void HAL::flushDisplay(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    if (area->x2 < 0 || area->y2 < 0) {
        return;
    }

    const u_long w = area->x2 - area->x1 + 1;
    const u_long h = area->y2 - area->y1 + 1;
    if (this->displayFlushState == 0) {
        digitalWrite(23, 1);

        gfx.startWrite();
        this->displayFlushState = 1;
    }

    this->pxl += w * h;
    if (xSemaphoreTake(this->displaySemaphore, (TickType_t)10) == pdTRUE) {
        gfx.setAddrWindow(area->x1, area->y1, w, h);
        gfx.pushPixels(static_cast<uint16_t*>(&color_p->full), w * h, true);
        if (lv_disp_flush_is_last(drv)) {
            gfx.endWrite();
            this->displayFlushState = 0;
            
            TaskHandle_t taskHandle = xTaskGetHandle("DelayedTask");
            if (taskHandle != NULL) {
                vTaskDelete(taskHandle);
            }

            if (!displayReady) {
                this->displayReady = true;
            }

            // todo rechnung passt nicht
            // lv_disp_enable_invalidation(NULL, true);

            if (this->pxl < SCREENWIDTH * SCREENHEIGHT) {
                xTaskCreate(delayedTask, "DelayedTask", 2048, NULL, 0, NULL);
            }
        }
        // gfx.sleep();

        xSemaphoreGive(this->displaySemaphore);

        lv_disp_flush_ready(drv);
    }
}

void HAL::readTouchScreen(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_REL;
    
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;

        TaskHandle_t taskHandle = xTaskGetHandle("pwrMngmntTask");
        if (taskHandle != nullptr) {
            xTaskNotify(taskHandle, 2, eSetValueWithOverwrite);
        }
    }
}

bool HAL::shouldFactoryReset() {
    pinMode(GPIO_NUM_39, INPUT_PULLUP);

    const TickType_t checkTime = pdMS_TO_TICKS(1 * 1000);

    int i = 0;
    while (!digitalRead(GPIO_NUM_39)) {
        i++;
        vTaskDelay(checkTime);
    }

    ESP_LOGV("HAL", "HOLD RESET FOR: %d Seconds", i);

    return i > 10;
}

void HAL::startSleep() {
    ESP_LOGV("BOOT", "START SLEEP UPTIME %u", millis());

    //  M5.disableEXTPower();
    digitalWrite(5, 0);
    gpio_hold_en((gpio_num_t)2);
    gpio_deep_sleep_hold_en();

    // HardwareBug ADC1 Triggert GPIO36
    esp_wifi_stop();
    if (this->adcInitialized) {
        adc_power_release();
    }

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, LOW);  // TOUCH_INT
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    if (xSemaphoreTake(this->displaySemaphore, (TickType_t)100) == pdTRUE) {
        gfx.waitDisplay();
        gfx.sleep();
    }
    vTaskDelay(100);

    // Workaround GT911 Status message
    uint16_t touchX, touchY;
    while (gfx.getTouch(&touchX, &touchY) || digitalRead(GPIO_NUM_36) == LOW) {
        vTaskDelay(100);
    };

    // EPD Power off
    digitalWrite(23, 0);
    esp_deep_sleep_start();
}