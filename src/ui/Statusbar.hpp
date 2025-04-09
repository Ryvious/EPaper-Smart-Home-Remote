#pragma once

class Statusbar {
   private:
    IHalInterface* hal;
    lv_obj_t* battery_label;
    lv_obj_t* time_label;
    lv_obj_t* wifi_label;

   public:
    Statusbar(IHalInterface* hal, lv_obj_t* parent) {
        this->hal = hal;

        // Statusbar erstellen
        lv_obj_t* statusbar = lv_obj_create(parent);
        lv_obj_set_width(statusbar, LV_PCT(100));
        lv_obj_set_height(statusbar, 50);  // Höhe der Statusleiste anpassen
        lv_obj_set_style_pad_right(statusbar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(statusbar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(statusbar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        // Akkuanzeige erstellen
        this->battery_label = lv_label_create(statusbar);
        // lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_3);
        lv_obj_align(this->battery_label, LV_ALIGN_LEFT_MID, 5, 0);  // Links in der Statusleiste positionieren

        // Uhrzeit erstellen
        this->time_label = lv_label_create(statusbar);
        lv_obj_align(this->time_label, LV_ALIGN_CENTER, 0, 0);  // In der Mitte der Statusleiste positionieren

        // WiFi-Signal erstellen
        this->wifi_label = lv_label_create(statusbar);
        lv_obj_align(this->wifi_label, LV_ALIGN_RIGHT_MID, -15, 0);  // Rechts in der Statusleiste positionieren

        this->update();
    };

    void update() {
        lv_label_set_text(this->wifi_label, getWifiLabel());

        const tm time = this->hal->getTime();
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%H:%M", &time);
        lv_label_set_text(this->time_label, buffer);
        lv_label_set_text(this->battery_label, getBatteryLabel());
    }

    const char* getBatteryLabel() {
        auto level = this->hal->getBatteryLevel();
        if (level >= 77) {
            return LV_SYMBOL_BATTERY_FULL;
        } else if (level >= 55) {
            return LV_SYMBOL_BATTERY_3;
        } else if (level >= 33) {
            return LV_SYMBOL_BATTERY_2;
        } else if (level >= 15) {
            return LV_SYMBOL_BATTERY_1;
        } else {
            return LV_SYMBOL_BATTERY_EMPTY;
        }
    }

    const char* getWifiLabel() {
        return this->hal->isNetworkConnected() ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING;
    }
};
