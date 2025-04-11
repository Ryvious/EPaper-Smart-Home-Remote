#include "HaProvider.hpp"

#include <ArduinoJson.h>
#include <esp_websocket_client.h>

#include "../PubSub.hpp"
#include "HaModel.hpp"
#include "IoTDevs.hpp"

static const char* TAG = "HAProvider";

using namespace std::placeholders;

HaProvider::HaProvider(DeviceStore* store, provider_settings_t settings, PubSub<DeviceCommand>* commandPubSub)
    : Provider(store, settings, commandPubSub) {
    ESP_LOGI(TAG, "Create HA Provider");
    this->isOnline = false;
    this->messageId = 1;

    esp_websocket_client_config_t websocket_cfg = {
        .host = this->settings.host.c_str(),
        .port = this->settings.port,
        .path = "/api/websocket",
        .buffer_size = 2048
    };

    if (this->settings.ssl) {
        websocket_cfg.cert_pem = this->settings.cert_pem.c_str();
        websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
        websocket_cfg.skip_cert_common_name_check = true;
    }

    this->client = esp_websocket_client_init(&websocket_cfg);
    auto lambda_websocket_event_handler = [](void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
        auto instance = static_cast<HaProvider*>(handler_args);
        if (instance) {
            instance->handleWebsocketEvent(base, event_id, event_data);
        }
    };
    esp_websocket_register_events(this->client, WEBSOCKET_EVENT_ANY, lambda_websocket_event_handler, (void*)this);
    esp_websocket_client_start(this->client);
}

void HaProvider::authenticate() {
    JsonDocument doc;
    doc["type"] = "auth";
    doc["access_token"] = this->settings.apiToken.c_str();

    std::string payload;
    serializeJson(doc, payload);
    esp_websocket_client_send_text(this->client, payload.c_str(), payload.length(), portMAX_DELAY);
}

void HaProvider::callService(const char* entity_id, const char* domain, const char* service, JsonDocument* serviceData) {
    JsonDocument doc;
    doc["id"] = this->messageId++;
    doc["type"] = "call_service";
    doc["domain"] = domain;
    doc["service"] = service;
    doc["service_data"]["entity_id"] = entity_id;
    if (serviceData != nullptr) {
        for (JsonPair kv : serviceData->as<JsonObject>()) {
            doc["service_data"][kv.key().c_str()] = kv.value();
        }
    }

    std::string output;
    serializeJson(doc, output);
    esp_websocket_client_send_text(this->client, output.c_str(), output.length(), portMAX_DELAY);
}

bool HaProvider::end() {
    this->isOnline = false;

    return esp_websocket_client_close(this->client, portMAX_DELAY);
};

void HaProvider::handle() {
    DeviceCommand command;
    if (this->commandPubSub->receive(this->clientId, command, portMAX_DELAY)) {
        this->process(this->store->getDevice(command.device_id), command);
    }
}

void HaProvider::handleWebsocketEvent(esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = static_cast<esp_websocket_event_data_t*>(event_data);
    if (!data) {
        ESP_LOGE(TAG, "Event data is null");
        return;
    }

    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        this->authenticate();
    } else if (event_id == WEBSOCKET_EVENT_DATA && data->data_ptr != nullptr) {
        DeserializationError error = deserializeJson(this->deserializationBuffer, data->data_ptr);
        if (error) {
            ESP_LOGE(TAG, "deserializeJson() %s: %s", error.c_str(), data->data_ptr);
            ESP_LOGE(TAG, "Free heap size: %d bytes", esp_get_free_heap_size());
            return;
        }

        const char* type = this->deserializationBuffer["type"];
        if (type == nullptr) {
            ESP_LOGE(TAG, "No 'type' property present in received data");
            ESP_LOGE(TAG, "Free heap size: %d bytes", esp_get_free_heap_size());
            return;
        }

        if (strcmp("auth_ok", type) == 0) {
            this->subscribeEntities();
        } else if (strcmp("event", type) == 0) {
            auto states = this->deserializationBuffer["event"].as<std::vector<EntityState>>();
            for (EntityState state : states) {
                this->event(this->store->getDevice(state.id), state);
            }

            if (!this->online() && deserializationBuffer["id"].as<int>() == this->subscriptionMessageId) {
                this->isOnline = true;
                ESP_LOGI(TAG, "HA Provider Online");
            }
        }
    }
}

void HaProvider::subscribeEntities() {
    if (this->store->getDeviceCount() <= 0) {
        ESP_LOGW(TAG, "No devices to subscribe");
        return;
    }

    JsonDocument doc;
    this->subscriptionMessageId = this->messageId++;
    doc["id"] = this->subscriptionMessageId;
    doc["type"] = "subscribe_entities";
    doc["entity_ids"].set(this->store->getDevices());

    std::string output;
    serializeJson(doc, output);
    esp_websocket_client_send_text(this->client, output.c_str(), output.length(), portMAX_DELAY);
}


template <> void HaProvider::event<Light>(Light* obj, EntityState data) {
    if (!data.state.empty()) {
        this->store->setState(obj, data.boolState());
    }
}

template <> void HaProvider::event<DimmLight>(DimmLight* obj, EntityState data) {
    if (int brightness; data.getIntAttribute("brightness", &brightness)) {
        obj->brightness = brightness / 2.55; // Attribute in HA 0-255
    }
    if (!data.state.empty()) {
        this->store->setState(obj, data.boolState());
    }
}

template <> void HaProvider::event<ColorLight>(ColorLight* obj, EntityState data) {
    this->event<DimmLight>(obj, data);
}

template <> void HaProvider::event<Climate>(Climate* obj, EntityState data) {
    if (!data.state.empty()) {
        this->store->setState(obj, data.boolState());
    }
}

template <> void HaProvider::event<Sensor>(Sensor* obj, EntityState data) {
    if (!data.state.empty()) {
        obj->value = data.state;
    }

    data.getStringAttribute("unit_of_measurement", &(obj->unit));
    this->store->notifyObservers(*obj);
}

template <> void HaProvider::event<Shutter>(Shutter* obj, EntityState data) {
    if (!data.state.empty()) {
        this->store->setState(obj, data.boolState());
    }
    if (int position; data.getIntAttribute("current_position", &position)) {
        obj->height = 100 - position;
    }

    this->store->notifyObservers(*obj);
}

template <> void HaProvider::process<Light>(Light* obj, DeviceCommand cmd) {
    auto action = static_cast<Light::supportedActions>(cmd.action);
    if (action == Light::supportedActions::setState) {
        const char* service = !obj->getState() ? "turn_on" : "turn_off";
        this->callService(obj->getId().c_str(), "light", service);
    }
}

template <> void HaProvider::process<DimmLight>(DimmLight* obj, DeviceCommand cmd) {
    auto action = static_cast<DimmLight::supportedActions>(cmd.action);
    if (action == DimmLight::supportedActions::setBrightness) {
        JsonDocument srvData;
        srvData["brightness"] = (int)(obj->brightness * 2.55);
        this->callService(obj->getId().c_str(), "light", "turn_on", &srvData);
    } else {
        this->process<Light>((Light*)obj, cmd);
    }
}

template <> void HaProvider::process<ColorLight>(ColorLight* obj, DeviceCommand cmd) {
    auto action = static_cast<ColorLight::supportedActions>(cmd.action);
    if (action == ColorLight::supportedActions::setColor) {
        JsonDocument srvData;
        srvData["rgb_color"] = obj->color.asVector();
        this->callService(obj->getId().c_str(), "light", "turn_on", &srvData);
    } else {
        this->process<DimmLight>((DimmLight*)obj, cmd);
    }
}

template <> void HaProvider::process<Climate>(Climate* obj, DeviceCommand cmd) {
    auto action = static_cast<Climate::supportedActions>(cmd.action);
    if (action == Climate::supportedActions::setState) {
        JsonDocument srvData;
        srvData["hvac_mode"] = !obj->getState() ? "auto" : "off";
        this->callService(obj->getId().c_str(), "climate", "set_hvac_mode", &srvData);
        obj->setState(!obj->getState());
    }
}

template <> void HaProvider::process<Shutter>(Shutter* obj, DeviceCommand cmd) {
    auto action = static_cast<Shutter::supportedActions>(cmd.action);
    JsonDocument srvData;
    if (action == Shutter::supportedActions::setHeight) {
        srvData["position"] = 100 - obj->height;
    } else {
        srvData["position"] = obj->getState() ? 100 : 0;
    }

    this->callService(obj->getId().c_str(), "cover", "set_cover_position", &srvData);
}