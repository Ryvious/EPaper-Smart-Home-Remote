#include "Ui.hpp"

#include <lvgl.h>

#include "../IotDevs.hpp"
#include "../PubSub.hpp"
#include "IHal.hpp"
#include "Statusbar.hpp"
#include "events/events.hpp"
#include "views/Menu.hpp"
#include "views/TileView.hpp"

static const char* TAG = "UI";

void UI::begin(std::vector<view_t> views, DeviceStore* store, PubSub<DeviceStatus>* statusPubSub, PubSub<DeviceCommand>* commandPubSub, IHalInterface* hal) {
    this->views = views;
    this->deviceStore = store;
    this->statusPubSub = statusPubSub;
    this->commandPubSub = commandPubSub;
    this->hal = hal;

    this->clientId = this->statusPubSub->subscribe();
}

void UI::draw(std::string viewname, bool blocking) {
    this->root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(this->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(this->root, LV_LAYOUT_GRID);

    static const lv_coord_t col_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const lv_coord_t row_dsc[] = { LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(this->root, col_dsc, row_dsc);

    this->container = lv_obj_create(this->root);
    lv_obj_remove_style_all(this->container);
    lv_obj_set_grid_cell(this->container, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t* btn_menu = lv_btn_create(this->root);
    lv_obj_set_size(btn_menu, 75, 75);
    lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_menu, LV_ALIGN_BOTTOM_LEFT, 0, 10);
    lv_obj_set_style_radius(btn_menu, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_img_src(btn_menu, LV_SYMBOL_LIST, 0);
    lv_obj_set_style_text_font(btn_menu, &lv_font_montserrat_44, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_menu, [](lv_event_t* e) { static_cast<UI*>(lv_event_get_user_data(e))->handleMenuClick(); }, LV_EVENT_CLICKED, this);

    this->manager = lv_fragment_manager_create(NULL);

    this->statusbar = new Statusbar(this->hal, this->root);

    if (viewname.empty()) {
        this->handleMenuClick();
    } else {
        this->namedView(viewname);
    }

    if (blocking) {
        while (!this->hal->isDisplayReady()) {
            lv_task_handler();
            vTaskDelay(50);
        }
    } else {
        BaseType_t uiTask = xTaskCreatePinnedToCore([](void* pvParameters) {
            auto instance = static_cast<UI*>(pvParameters);
            for (;;) {
                instance->handle();
            }
        }, "UITask", 8000, this, 4, NULL, APP_CPU_NUM);
    }
}

void UI::handle() {
    DeviceStatus status;
    if (this->statusPubSub->receive(this->clientId, status, pdMS_TO_TICKS(100))) {
        // this->statusbar->update();
        lv_msg_send(UIEvents::DEV_VALUE_CHANGE, status.device_id);
    }

    lv_task_handler();
}

void UI::handleMenuClick() {
    auto callback = [this](std::string arg) {
        this->namedView(arg);
    };

    menu_fragment_args_t args = {
        .views = &this->views,
        .callback = callback
    };
    lv_fragment_t* fragment = lv_fragment_create(&menu_cls, &args);
    this->pushFragment(fragment);
}

void UI::namedView(std::string viewname) {
    ESP_LOGD(TAG, "Route clicked: %s", viewname.c_str());
    lv_fragment_manager_pop(this->manager);

    auto it = std::find_if(this->views.begin(), this->views.end(), [viewname](const view_t& view) {
        return view.name == viewname;
    });

    if (it != this->views.end()) {
        ESP_LOGV(TAG, "Route found");
        if (this->stateChangeCallback) {
            this->stateChangeCallback(viewname);
        }

        tile_fragment_args_t args = {
            .viewPtr = &*it,
            .store = this->deviceStore,
            .commandPubSub = this->commandPubSub
        };
        lv_fragment_t* fragment = lv_fragment_create(&tile_cls, &args);
        this->pushFragment(fragment);
    } else {
        ESP_LOGE(TAG, "Route '%s' was not found", viewname.c_str());
    }
}

void UI::pushFragment(lv_fragment_t* fragment) {
    lv_fragment_manager_push(this->manager, fragment, &this->container);
}

void UI::setStateChangeCallback(StateChangeCallback stateChangeCallback) {
    this->stateChangeCallback = stateChangeCallback;
}