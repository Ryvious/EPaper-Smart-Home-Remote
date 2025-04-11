#include "SettingsManager.hpp"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

#include "model/settings/Settings.hpp"

static const char* TAG = "Settings";

SettingsManager::~SettingsManager() {
    this->preferences.end();
}

void SettingsManager::begin() {
    preferences.begin("EPDSH", false, "settings");

    auto chunks = preferences.getInt("chunks", 0);
    if (chunks > 0) {
        String settingsString = "";
        for (int c = 0; c < chunks; c++) {
            settingsString += preferences.getString(String(c).c_str(), "");
        }

        JsonDocument doc;
        if (!deserializeJson(doc, settingsString, DeserializationOption::NestingLimit(15))) {
            this->settings = doc.as<settings_t>();
            return;
        }
    }

    ESP_LOGW(TAG, "Load default settings");
    this->loadDefaults();
}

bool SettingsManager::commit() {
    JsonDocument doc;
    doc.set(this->settings);

    String jsonOutput;
    serializeJson(doc, jsonOutput);
    int length = jsonOutput.length();
    if (length <= 0) {
        return false;
    }

    preferences.clear();

    int chunks = 0;
    do {
        auto beginIndex = chunks * MAX_NVS_CHUNK_SIZE, endIndex = beginIndex + min(length, MAX_NVS_CHUNK_SIZE);
        auto chunk = jsonOutput.substring(beginIndex, endIndex);
        auto size = preferences.putString(String(chunks).c_str(), chunk.c_str());
        if (size == 0) {
            ESP_LOGE(TAG, "Commiting NVS chunk %d failed", chunks);
            return false;
        }

        ESP_LOGV(TAG, "Committed NVS chunk %d of length %d: %s", chunks, size, chunk.c_str());
        length -= size;
        chunks++;
    } while (length > 0);

    preferences.putInt("chunks", chunks);

    return chunks > 0;
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
        vTaskDelay(500);
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