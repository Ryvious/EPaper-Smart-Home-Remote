#include "WiFiManager.hpp"

#include <ArduinoJson.h>
#include <AsyncJson.h>
//#include <NTP.h>
#include <WiFi.h>
//#include <WiFiUdp.h>
#include <esp_wifi.h>

static const char* TAG = "WiFI";

WiFiManager::WiFiManager(WiFiClass* wifi, const wifimanager_settings_t* settings)
    : wifi(wifi), settings(settings) {
    this->wifi->begin();

    this->wifi->onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        system_event_sta_connected_t e = info.wifi_sta_connected;
        ESP_LOGI(TAG, "Connected to %s", (char*)e.ssid);
    }, ARDUINO_EVENT_WIFI_STA_CONNECTED);

    this->wifi->onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
        system_event_sta_got_ip_t e = info.got_ip;
        ESP_LOGI(TAG, "Got IP: %s", IPAddress(e.ip_info.ip.addr).toString().c_str());
        ESP_LOGI(TAG, "Got NM: %s", IPAddress(e.ip_info.netmask.addr).toString().c_str());
        ESP_LOGI(TAG, "Got GW: %s", IPAddress(e.ip_info.gw.addr).toString().c_str());
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

    this->wifi->onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        system_event_sta_disconnected_t e = info.wifi_sta_disconnected;
        ESP_LOGI(TAG, "Disconnected from SSID: %s\tReason: %d", ((char*)e.ssid), e.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    this->wifi->onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        system_event_ap_probe_req_rx_t e = info.wifi_ap_probereqrecved;
        ESP_LOGI(TAG, "Probe received: %.2X:%.2X:%.2X:%.2X:%.2X:%.2X (%d)", e.mac[0], e.mac[1], e.mac[2], e.mac[3], e.mac[4], e.mac[5], e.rssi);
    }, ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED);

    this->wifi->onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        system_event_ap_staconnected_t e = info.wifi_ap_staconnected;
        ESP_LOGI(TAG, "Client %.2X:%.2X:%.2X:%.2X:%.2X:%.2X (%d) connected", e.mac[0], e.mac[1], e.mac[2], e.mac[3], e.mac[4], e.mac[5], e.aid);
    }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

    this->wifi->onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        system_event_ap_stadisconnected_t e = info.wifi_ap_stadisconnected;
        ESP_LOGI(TAG, "Client %.2X:%.2X:%.2X:%.2X:%.2X:%.2X (%d) disconnected", e.mac[0], e.mac[1], e.mac[2], e.mac[3], e.mac[4], e.mac[5], e.aid);
    }, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
}

bool WiFiManager::begin(bool blocking) {
    // this->wifi->mode(WIFI_MODE_STA);
    // this->wifi->setSleep(false);
    // this->wifi->setAutoReconnect(true);

    // this->ntp = new NTP(this->wifiUdp);
    // this->ntp->ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
    // this->ntp->ruleSTD("CET", Last, Sun, Oct, 3, 60);   // last sunday in october 3:00, timezone +60min (+1 GMT)
    // this->ntp->begin("0.de.pool.ntp.org"); */

    // this->wifi->setHostname(hostname);
    // this->wifi->softAPsetHostname(hostname);

    if (this->wifi->status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "WIFI Connected");
        return true;
    }

    this->end();
    if (this->settings->sta_ssid.length() > 0 /*&& this->settings->sta_password.length() > 0*/) {
        ESP_LOGI(TAG, "WIFI not connected, configuring STA mode");
        this->wifi->mode(WIFI_MODE_STA);
        this->wifi->persistent(true);
        this->wifi->setAutoConnect(true);

        if (strcmp(this->wifi->SSID().c_str(), this->settings->sta_ssid.c_str()) != 0 ||
            strcmp(this->wifi->psk().c_str(), this->settings->sta_password.c_str()) != 0) {
            ESP_LOGD(TAG, "WIFI old STA config: %s %s", this->wifi->SSID().c_str(), this->wifi->psk().c_str());
            ESP_LOGD(TAG, "WIFI new STA config: %s %s", this->settings->sta_ssid.c_str(), this->settings->sta_password.c_str());

            this->wifi->begin(this->settings->sta_ssid.c_str(), this->settings->sta_password.c_str());
            // this->wifi->persistent(false);
        }
    } else if (this->settings->ap_ssid.length() > 0 /*&& this->settings->ap_password.length() > 0*/) {
        ESP_LOGI(TAG, "WIFI not connected, configuring AP mode");
        this->wifi->mode(WIFI_MODE_AP);

        ESP_LOGD(TAG, "WIFI AP config: %s %s", this->settings->ap_ssid.c_str(), this->settings->ap_password.c_str());
        this->wifi->softAP(this->settings->ap_ssid.c_str(), this->settings->ap_password.c_str());
    } else {
        this->wifi->mode(WIFI_MODE_NULL);
    }

    auto wifiTaskLambda = [](void* pvParameters) {
        auto instance = static_cast<WiFiManager*>(pvParameters);
        for (;;) {
            vTaskDelay(10000);
            instance->handle();
        }
    };
    BaseType_t testTask = xTaskCreatePinnedToCore(wifiTaskLambda, "wifiTask", 2000, this, 10, NULL, PRO_CPU_NUM);

    if (blocking) {
        if (WiFi.waitForConnectResult() != WL_CONNECTED) {
            ESP_LOGE(TAG, "WiFi Failed!");
        }
    }

    return this->wifi->getMode() != WIFI_MODE_NULL;
}

void WiFiManager::end() {
    TaskHandle_t taskHandle = xTaskGetHandle("wifiTask");
    if (taskHandle != NULL) {
        vTaskDelete(taskHandle);
    }

    this->wifi->disconnect();
    this->wifi->softAPdisconnect();
}

void WiFiManager::handle() {
    if (!this->wifi->isConnected() && (this->wifi->getMode() & WIFI_MODE_AP) != WIFI_MODE_AP) {
        ESP_LOGW(TAG, "Not connected to STA falling back to AP mode");
        this->wifi->mode(WIFI_MODE_AP);
        this->wifi->softAP(this->settings->ap_ssid.c_str(), this->settings->ap_password.c_str());
    }
}

/*
void WiFiManager::queryNtp() {
    ESP_LOGI(TAG, "GET NTP");
    if (!this->ntp->update()) {
        ESP_LOGE(TAG, "NTP ERROR");  // Www hh:mm:ss
    }

    const char* dt = this->ntp->formattedTime("%A %T");
    ESP_LOGI(TAG, "%s", dt);  // Www hh:mm:ss
}
*/

void WiFiManager::setupRestApi(AsyncWebServer* webserver, const char* url) {
    webserver->on(url, HTTP_GET, [this](AsyncWebServerRequest* request) {
        AsyncJsonResponse* response = new AsyncJsonResponse(true);
        response->addHeader("Access-Control-Allow-Origin", "*");

        JsonArray root = response->getRoot();

        int n = WiFi.scanComplete();
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                JsonObject jsNetwork = root.createNestedObject();
                jsNetwork["SSID"] = WiFi.SSID(i);
                jsNetwork["RSSI"] = WiFi.RSSI(i);
            }
        }

        response->setLength();
        request->send(response);
    });
}