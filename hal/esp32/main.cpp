#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc.h>

#include <WebContent.cpp>

#include "AppState.hpp"
#include "HAL.hpp"
#include "IoTDevs.hpp"
#include "PowerManager.hpp"
#include "Provider/ProviderFactory.hpp"
#include "PubSub.hpp"
#include "SettingsManager.hpp"
#include "SettingsWebServer.hpp"
#include "WifiManager.hpp"
#include "ui/Ui.hpp"

static const auto TAG = "MAIN";

HAL hal;
AppState appState;
settings_t settings;
SettingsManager settingsManager;
SettingsWebServer settingsWebServer;
PowerManager* powerManager;
WiFiManager* wifiManager;
PubSub<DeviceCommand> commandPubSub;
PubSub<DeviceStatus> statusPubSub;
DeviceStore store(&statusPubSub);
Provider* provider;
UI ui;

void appShutdown();
bool hasPendingChanges();
void setupWebServer(void);
void setupUserInterface(const settings_t& settings, bool drawOnce);

void setup(void) {
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    ESP_LOGI(TAG, ">>> Booting @%dMHz with reason %d ===", F_CPU / 1000L / 1000L, appState.getWakeupReason());
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    bool synchronousStart = appState.isWakeupReason(ESP_SLEEP_WAKEUP_TIMER);

    settingsManager.begin();
    if (hal.shouldFactoryReset()) {
        settingsManager.loadDefaults();
    }
    settings = settingsManager.getSettings();

    powerManager = new PowerManager(appShutdown);

    wifiManager = new WiFiManager(&WiFi, &settings.wifi);
    wifiManager->begin(synchronousStart);

    store.initFromSettings(settings.devices);

    provider = ProviderFactory::create(settings.provider, &store, settings.providerParams, &commandPubSub);
    provider->begin(synchronousStart);

    if (synchronousStart && !hasPendingChanges()) {  // go back to sleep if nothing has changed
        hal.startSleep();
        return;
    }

    hal.init(synchronousStart);

    setupUserInterface(settings, synchronousStart);
    setupWebServer();
}

void loop(void) {
    vTaskDelete(NULL);
}

// Shutdown Task PowerManager
void appShutdown() {
    provider->end();
    std::string viewname = appState.getUiState();
    uint32_t chkSum = store.getChecksum(settings.getDevicesByView(viewname));
    appState.setChecksum(chkSum);
    hal.startSleep();
}

// Checksum diff check
bool hasPendingChanges() {
    std::string viewname = appState.getUiState();
    uint32_t chkSum = store.getChecksum(settings.getDevicesByView(viewname));

    return (chkSum != appState.getChecksum());
}

void setupUserInterface(const settings_t& settings, bool drawOnce) {
    AppState state = appState;

    ui.begin(settings.views, &store, &statusPubSub, &commandPubSub, &hal);
    ui.setStateChangeCallback([&state](const std::string& message) { state.setUiState(message); });
    ui.draw(state.getUiState(), drawOnce);
    if (drawOnce) {
        hal.startSleep();  // todo PWR Manager
    }
}

void setupWebServer(void) {
    WebContent::setupStaticWebContent(settingsWebServer.getWebServer());
    settingsWebServer.registerRestApi(&settingsManager, "/api/settings");
    settingsWebServer.registerRestApi(wifiManager, "/api/wifi");
    settingsWebServer.begin();
}