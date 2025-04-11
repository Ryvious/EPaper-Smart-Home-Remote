#pragma once

#include <lvgl.h>

#include "../IoTDevs.hpp"
#include "../PubSub.hpp"
#include "../model/settings/views.hpp"
#include "IHal.hpp"
#include "Statusbar.hpp"

using StateChangeCallback = std::function<void(const std::string&)>;

class UI {
private:
    int clientId;
    lv_obj_t* container;
    lv_obj_t* label;
    lv_fragment_manager_t* manager;
    lv_obj_t* root;
    std::vector<view_t> views;

    IHalInterface* hal;
    DeviceStore* deviceStore;
    PubSub<DeviceCommand>* commandPubSub;
    PubSub<DeviceStatus>* statusPubSub;
    StateChangeCallback stateChangeCallback;
    Statusbar* statusbar;

public:
    void begin(std::vector<view_t> views, DeviceStore* store, PubSub<DeviceStatus>* statusPubSub, PubSub<DeviceCommand>* commandPubSub, IHalInterface* hal);

    void draw(std::string viewname, bool blocking = false);

    void setStateChangeCallback(StateChangeCallback stateChangeCallback);

private:
    void handle();

    void handleMenuClick();

    void namedView(std::string viewname);

    void pushFragment(lv_fragment_t* fragment);
};