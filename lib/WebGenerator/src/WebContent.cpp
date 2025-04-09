
#include <ESPAsyncWebServer.h>
#include <WebContent.hpp>

class WebContent {
  public:
    static void setupStaticWebContent(AsyncWebServer* webserver) {
        webserver->rewrite("/", "/index.html");

        webserver->on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request){
            request->send_P(200, F("text/html"), index_html, 370);
        });

        webserver->on("/plugin.css", HTTP_GET, [](AsyncWebServerRequest* request){
            AsyncWebServerResponse* response = request->beginResponse_P(200, F("text/css"), plugin_css_gz, 40233);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/plugin.js", HTTP_GET, [](AsyncWebServerRequest* request){
            AsyncWebServerResponse* response = request->beginResponse_P(200, F("application/javascript"), plugin_js_gz, 86958);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
    }
};
