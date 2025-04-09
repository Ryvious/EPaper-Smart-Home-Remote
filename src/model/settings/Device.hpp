#pragma once
#include <ArduinoJson.h>
#include <esp_wifi_types.h>

#include <iostream>
#include <map>
#include <vector>

struct device_t {
    std::string id;
    std::string name;
    std::string type;
};

namespace ArduinoJson {
    template <> struct Converter<device_t> {
        static bool toJson(const device_t& src, JsonVariant dst) {
            dst["name"] = src.name;
            dst["type"] = src.type;
            dst["id"] = src.id;

            return true;
        }

        static device_t fromJson(JsonVariantConst src) {
            device_t dst;
            dst.name = src["name"].as<std::string>();
            dst.type = src["type"].as<std::string>();
            dst.id = src["id"].as<std::string>();

            return dst;
        }

        static bool checkJson(JsonVariantConst src) {
            return true;
        }
    };
}  // namespace ArduinoJson