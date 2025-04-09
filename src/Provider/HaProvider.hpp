#pragma once
#include "../model/settings/Provider.hpp"
#include "HaModel.hpp"
#include "IoTDevs.hpp"
#include "Provider.hpp"
#include "esp_websocket_client.h"

class HaProvider : public Provider {
   private:
    esp_websocket_client_handle_t client;
    JsonDocument deserializationBuffer;
    bool isOnline = false;
    int messageId, subscriptionMessageId;

   public:
    HaProvider(DeviceStore* store, provider_settings_t settings, PubSub<DeviceCommand>* commandPubSub);

    bool end() override;
    void handle() override;
    bool online() override { return this->isOnline; }

   private:
    void authenticate();
    void callService(const char* entity_id, const char* domain, const char* service, JsonDocument* serviceData = nullptr);
    void handleWebsocketEvent(esp_event_base_t base, int32_t event_id, void* event_data);
    void subscribeEntities();

    template <typename T> void event(T* obj, EntityState data) {
        static_assert(std::is_base_of<IoTDevice, T>::value, "T must be a derived class of Base");
        ESP_LOGI("HaProvider", "General event for type: %s", typeid(T).name());

        if (obj == nullptr) {
            return;
        }

        static_cast<IoTDevice*>(obj)->setChecksum(data.lastChange);
        if (ColorLight* d = dynamic_cast<ColorLight*>(obj); d != nullptr) {
            this->event<ColorLight>(d, data);
        } else if (DimmLight* d = dynamic_cast<DimmLight*>(obj); d != nullptr) {
            this->event<DimmLight>(d, data);
        } else if (Light* d = dynamic_cast<Light*>(obj); d != nullptr) {
            this->event<Light>(d, data);
        } else if (Sensor* d = dynamic_cast<Sensor*>(obj); d != nullptr) {
            this->event<Sensor>(d, data);
        } else if (Climate* d = dynamic_cast<Climate*>(obj); d != nullptr) {
            this->event<Climate>(d, data);
        } else if (Shutter* d = dynamic_cast<Shutter*>(obj); d != nullptr) {
            this->event<Shutter>(d, data);
        } else {
            ESP_LOGE("HaProvider", "General processing for unkown type!");
        }
    }

    template <typename T> void process(T* obj, DeviceCommand cmd) {
        static_assert(std::is_base_of<IoTDevice, T>::value, "T must be a derived class of Base");
        ESP_LOGI("HaProvider", "General processing for type: %s", typeid(T).name());

        if (ColorLight* colorLight = dynamic_cast<ColorLight*>(obj)) {
            this->process<ColorLight>(colorLight, cmd);
        } else if (DimmLight* dimmLight = dynamic_cast<DimmLight*>(obj)) {
            this->process<DimmLight>(dimmLight, cmd);
        } else if (Light* light = dynamic_cast<Light*>(obj)) {
            this->process<Light>(light, cmd);
        } else if (Climate* climate = dynamic_cast<Climate*>(obj)) {
            this->process<Climate>(climate, cmd);
        } else if (Sensor* sensor = dynamic_cast<Sensor*>(obj)) {
            this->process<Sensor>(sensor, cmd);
        } else if (Shutter* shutter = dynamic_cast<Shutter*>(obj)) {
            this->process<Shutter>(shutter, cmd);
        }
    }
};