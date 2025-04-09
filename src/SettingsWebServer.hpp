#pragma once
#include <ESPAsyncWebServer.h>

#include "IRestApiSetup.hpp"

class SettingsWebServer {
   public:
    SettingsWebServer() : server(80) {
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
    }

    void begin() {
        server.begin();
    }

    void registerRestApi(IRestApiSetup* apiSetupInstance, const char* url) {
        if (apiSetupInstance != nullptr) {
            apiSetupInstance->setupRestApi(this->getWebServer(), url);
        }
    }

    AsyncWebServer* getWebServer() {
        return &server;
    }

   private:
    AsyncWebServer server;
};