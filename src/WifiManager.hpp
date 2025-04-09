#pragma once
#include <ArduinoJson.h>
//#include <NTP.h>
#include <WiFi.h>

#include "IRestApiSetup.hpp"

struct wifimanager_settings_t {
    std::string ap_ssid = "EPDSH";
    std::string ap_password = "EPDSH.123";
    std::string sta_ssid = "";
    std::string sta_password = "";
};

namespace ArduinoJson {
    template <> struct Converter<wifimanager_settings_t> {
        static bool toJson(const wifimanager_settings_t& src, JsonVariant dst) {
            dst["ap_ssid"] = src.ap_ssid;
            dst["ap_password"] = src.ap_password;
            dst["sta_ssid"] = src.sta_ssid;
            dst["sta_password"] = src.sta_password;

            return true;
        }

        static wifimanager_settings_t fromJson(JsonVariantConst src) {
            wifimanager_settings_t dst;
            dst.ap_ssid = src["ap_ssid"].as<std::string>();
            dst.ap_password = src["ap_password"].as<std::string>();
            dst.sta_ssid = src["sta_ssid"].as<std::string>();
            dst.sta_password = src["sta_password"].as<std::string>();

            return dst;
        }

        static bool checkJson(JsonVariantConst src) {
            return true;
        }
    };
}  // namespace ArduinoJson

class WiFiManager : public IRestApiSetup {
   private:
    const wifimanager_settings_t* settings;
    WiFiClass* wifi;
    //WiFiUDP wifiUdp;
    //NTP* ntp;

   public:
    WiFiManager(WiFiClass* wifi, const wifimanager_settings_t* settings);

    bool begin(bool blocking = false);
    void end();
    void handle();
    //void queryNtp();

    void setupRestApi(AsyncWebServer* webserver, const char* url) override;
};