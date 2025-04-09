#pragma once
#include "HaProvider.hpp"
#include "Provider.hpp"

class ProviderFactory {
   public:
    static Provider* create(const std::string& type, DeviceStore* store, provider_settings_t settings, PubSub<DeviceCommand>* commandPubSub) {
        if (type == "HA") {
            return new HaProvider(store, settings, commandPubSub);
        }

        return nullptr;
    }
};