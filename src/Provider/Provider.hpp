#pragma once
#include "../PubSub.hpp"
#include "IoTDevs.hpp"

class Provider {
   protected:
    DeviceStore* store;
    provider_settings_t settings;
    PubSub<DeviceCommand>* commandPubSub;
    int clientId;

   public:
    Provider(DeviceStore* store, provider_settings_t settings, PubSub<DeviceCommand>* commandPubSub) {
        this->store = store;
        this->settings = settings;
        this->commandPubSub = commandPubSub;

        this->clientId = this->commandPubSub->subscribe();
    };

    bool begin(bool blocking) {
        auto networkTaskLambda = [](void* pvParameters) {
            if (pvParameters == nullptr) {
                ESP_LOGE("PROVIDER", "Provider Config error");
            }

            TickType_t xLastWakeTime = xTaskGetTickCount();
            const TickType_t xFrequency = pdMS_TO_TICKS(20);

            auto* instance = static_cast<Provider*>(pvParameters);
            for (; instance != nullptr;) {
                instance->handle();

                vTaskDelayUntil(&xLastWakeTime, xFrequency);
            }
        };

        BaseType_t _networkTask = xTaskCreatePinnedToCore(networkTaskLambda, "networkTask", 10000, this, 1, NULL, PRO_CPU_NUM);
        if (_networkTask != pdPASS) {
            return false;
        }

        while (blocking && !(this->online())) {
            vTaskDelay(50);
        }

        return true;
    }

    virtual bool end() = 0;

    virtual void handle() = 0;

    virtual bool online() = 0;
};