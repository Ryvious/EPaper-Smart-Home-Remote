#pragma once
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

#include "IRestApiSetup.hpp"
#include "model/settings/Settings.hpp"

class SettingsManager : public IRestApiSetup {
   private:
    Preferences preferences;
    settings_t settings;

   public:
    ~SettingsManager();

    void begin();
    bool commit();
    void loadDefaults();

    void setupRestApi(AsyncWebServer* webserver, const char* url) override;

    settings_t getSettings();
    bool setSettings(settings_t settings);
};