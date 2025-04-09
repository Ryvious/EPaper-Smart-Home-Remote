#pragma once
#include <ArduinoJson.h>

#include <iostream>
#include <map>
#include <vector>

#include "Device.hpp"
#include "Provider.hpp"
#include "StlConverter.hpp"
#include "Views.hpp"
#include "WifiManager.hpp"

struct webCredentials_t {
    std::string username = "admin";
    std::string password = "admin";
};

struct settings_t {
    std::string provider = "HA";
    std::vector<device_t> devices;
    wifimanager_settings_t wifi;
    provider_settings_t providerParams;
    std::vector<view_t> views;
    webCredentials_t webLogin;

    std::vector<std::string> getDevicesByView(const std::string& viewname) const {
        auto it = std::find_if(this->views.begin(), this->views.end(), [viewname](const view_t& obj) {
            return obj.name == viewname;
        });

        if (it != this->views.end()) {
            const view_t& foundView = *it;

            return foundView.devices;
        }

        return std::vector<std::string>();
    };
};

namespace ArduinoJson {
    template <> struct Converter<webCredentials_t> {
        static bool toJson(const webCredentials_t& src, JsonVariant dst) {
            dst["username"] = src.username;
            dst["password"] = src.password;

            return true;
        }

        static webCredentials_t fromJson(JsonVariantConst src) {
            webCredentials_t dst;
            dst.username = src["username"].as<std::string>();
            dst.password = src["password"].as<std::string>();

            return dst;
        }

        static bool checkJson(JsonVariantConst src) {
            return true;
        }
    };

    template <> struct Converter<settings_t> {
        static bool toJson(const settings_t& src, JsonVariant dst) {
            dst["provider"] = src.provider;
            dst["devices"] = src.devices;
            dst["wifi"] = src.wifi;
            dst["views"] = src.views;
            dst["providerParams"] = src.providerParams;
            dst["webLogin"] = src.webLogin;

            return true;
        }

        static settings_t fromJson(JsonVariantConst src) {
            settings_t dst;
            dst.provider = src["provider"].as<std::string>();
            dst.devices = src["devices"].as<std::vector<device_t>>();
            dst.wifi = src["wifi"].as<wifimanager_settings_t>();
            dst.views = src["views"].as<std::vector<view_t>>();
            dst.providerParams = src["providerParams"].as<provider_settings_t>();
            dst.webLogin = src["webLogin"].as<webCredentials_t>();

            return dst;
        }

        static bool checkJson(JsonVariantConst src) {
            return true;
        }
    };
}  // namespace ArduinoJson