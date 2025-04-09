#include "SettingsManager.hpp"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

#include "model/settings/Settings.hpp"

static const char* TAG = "Settings";
static const char* NVS_KEY = "EPDSH";

SettingsManager::~SettingsManager() {
    this->preferences.end();
}

void SettingsManager::begin() {
    preferences.begin(NVS_KEY, false, "settings");
    String settingsString = preferences.getString(NVS_KEY, "");
    if (settingsString.isEmpty()) {
        ESP_LOGW(TAG, "Load Default settings");
        this->loadDefaults();
    } else {
        JsonDocument doc;
        deserializeJson(doc, settingsString, DeserializationOption::NestingLimit(15));
        this->settings = doc.as<settings_t>();
    }
}

bool SettingsManager::commit() {
    JsonDocument doc;
    doc.set(this->settings);

    std::string jsonOutput;
    serializeJson(doc, jsonOutput);
    ESP_LOGI(TAG, "Serialized Settings: %s", jsonOutput.c_str());

    preferences.remove(NVS_KEY);
    size_t size = preferences.putString(NVS_KEY, jsonOutput.c_str());
    ESP_LOGV(TAG, "Committed NVS-Size: %d", size);

    return (size != 0);
}

void SettingsManager::loadDefaults() {
    settings_t settings;

    wifimanager_settings_t wifi;
    settings.wifi = wifi;

    webCredentials_t webLogin;
    settings.webLogin = webLogin;

    this->setSettings(settings);
}

void SettingsManager::setupRestApi(AsyncWebServer* webserver, const char* url) {
    auto createResponse = []() -> AsyncJsonResponse* {
        auto response = new AsyncJsonResponse(false);
        response->addHeader("Access-Control-Allow-Origin", "*");
        return response;
    };

    webserver->on(url, HTTP_GET, [this, createResponse](AsyncWebServerRequest* request) {
        auto response = createResponse();
        JsonObject root = response->getRoot();
        const settings_t settings = this->getSettings();
        Converter<settings_t>().toJson(settings, root);
        response->setLength();
        request->send(response);
    }).setAuthentication(this->settings.webLogin.username.c_str(), this->settings.webLogin.password.c_str());

    webserver->on(url, HTTP_OPTIONS, [createResponse](AsyncWebServerRequest* request) {
        auto response = createResponse();
        response->setLength();
        request->send(response);
    });

    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(url, [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!request->authenticate(this->settings.webLogin.username.c_str(), this->settings.webLogin.password.c_str())) {
            return request->requestAuthentication();
        }

        settings_t settings = json.as<settings_t>();
        this->setSettings(settings);
        request->send(200);
        vTaskDelay(200);
        ESP.restart();
    });

    webserver->addHandler(handler);
}

settings_t SettingsManager::getSettings() {
    return this->settings;
}
bool SettingsManager::setSettings(settings_t settings) {
    this->settings = settings;

    return this->commit();
}