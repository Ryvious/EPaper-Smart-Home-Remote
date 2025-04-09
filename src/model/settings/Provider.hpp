#pragma once
#include <ArduinoJson.h>

class provider_settings_t {
   public:
    std::string host = "";
    uint16_t port = 0;
    std::string apiToken = "";
    bool ssl = false;
    std::string cert_pem = "";
};

namespace ArduinoJson {
    template <> struct Converter<provider_settings_t> {
        static bool toJson(const provider_settings_t& src, JsonVariant dst) {
            dst["host"] = src.host;
            dst["port"] = src.port;
            dst["apiToken"] = src.apiToken;
            dst["ssl"] = src.ssl;
            dst["cert_pem"] = src.cert_pem;
            
            return true;
        }

        static provider_settings_t fromJson(JsonVariantConst src) {
            provider_settings_t dst;
            dst.host = src["host"].as<std::string>();
            dst.port = src["port"].as<int>();
            dst.apiToken = src["apiToken"].as<std::string>();
            dst.ssl = src["ssl"].as<bool>();
            dst.cert_pem = src["cert_pem"].as<std::string>();

            return dst;
        }

        static bool checkJson(JsonVariantConst src) {
            return true;
        }
    };
}  // namespace ArduinoJson