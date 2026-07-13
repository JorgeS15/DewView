#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>

#include "dewview_config.h"
#include "dewview_net.h"

static WebServer s_server(80);
static S24Reading s_last_reading = {};
static bool s_last_ok = false;
static volatile bool s_ota_busy = false;

/*------------------------------ Paginas web ------------------------------*/

static void handle_root()
{
    char body[640];
    if (s_last_ok) {
        snprintf(body, sizeof(body),
                 "<h1>DewView</h1>"
                 "<p>Temperatura: <b>%.1f &deg;C</b></p>"
                 "<p>Ponto de orvalho: <b>%.1f &deg;C</b></p>"
                 "<p>Humidade: <b>%.1f %%HR</b></p>"
                 "<p>Margem T-Td: <b>%.1f &deg;C</b></p>"
                 "<p><a href=\"/update\">Atualizar firmware</a></p>",
                 s_last_reading.tempC, s_last_reading.dewC,
                 s_last_reading.humidity, s_last_reading.tempC - s_last_reading.dewC);
    } else {
        snprintf(body, sizeof(body),
                 "<h1>DewView</h1><p>Sem dados do sensor.</p>"
                 "<p><a href=\"/update\">Atualizar firmware</a></p>");
    }
    char page[832];
    snprintf(page, sizeof(page),
             "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
             "<meta http-equiv=\"refresh\" content=\"5\">"
             "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
             "<title>DewView</title></head>"
             "<body style=\"font-family:sans-serif\">%s</body></html>", body);
    s_server.send(200, "text/html", page);
}

static bool check_auth()
{
    if (!s_server.authenticate(DEWVIEW_OTA_USER, DEWVIEW_OTA_PASSWORD)) {
        s_server.requestAuthentication();
        return false;
    }
    return true;
}

static void handle_update_form()
{
    if (!check_auth()) {
        return;
    }
    s_server.send(200, "text/html",
                  "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                  "<title>DewView OTA</title></head>"
                  "<body style=\"font-family:sans-serif\">"
                  "<h1>Atualizar firmware</h1>"
                  "<p>Escolhe o ficheiro .bin (Sketch &gt; Export Compiled Binary no Arduino IDE).</p>"
                  "<form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">"
                  "<input type=\"file\" name=\"firmware\" accept=\".bin\">"
                  "<input type=\"submit\" value=\"Atualizar\">"
                  "</form></body></html>");
}

static void handle_update_done()
{
    if (!check_auth()) {
        return;
    }
    s_ota_busy = false;
    if (Update.hasError()) {
        s_server.send(500, "text/html",
                      "<meta charset=\"utf-8\">Falha na atualizacao. Ver monitor serie.");
    } else {
        s_server.client().setNoDelay(true);
        s_server.send(200, "text/html",
                      "<meta charset=\"utf-8\">Atualizado com sucesso. A reiniciar...");
        delay(500);
        ESP.restart();
    }
}

static void handle_update_upload()
{
    HTTPUpload &upload = s_server.upload();

    switch (upload.status) {
    case UPLOAD_FILE_START:
        s_ota_busy = true;
        Serial.printf("OTA web: inicio (%s)\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
        break;
    case UPLOAD_FILE_WRITE:
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        break;
    case UPLOAD_FILE_END:
        if (Update.end(true)) {
            Serial.printf("OTA web: concluido, %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
        break;
    case UPLOAD_FILE_ABORTED:
        Update.abort();
        s_ota_busy = false;
        Serial.println("OTA web: abortado");
        break;
    default:
        break;
    }
}

/*--------------------------------- API -----------------------------------*/

void dewview_net_begin()
{
#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    /* AP proprio + ligacao a rede do gateway Modbus */
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(DEWVIEW_WIFI_SSID, DEWVIEW_WIFI_PASSWORD);
    WiFi.setAutoReconnect(true);
#else
    WiFi.mode(WIFI_AP);
#endif
    WiFi.softAP(DEWVIEW_AP_SSID, DEWVIEW_AP_PASSWORD);
    Serial.printf("AP \"%s\" ativo, IP %s\n",
                  DEWVIEW_AP_SSID, WiFi.softAPIP().toString().c_str());

    if (MDNS.begin(DEWVIEW_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
    }

    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/update", HTTP_GET, handle_update_form);
    s_server.on("/update", HTTP_POST, handle_update_done, handle_update_upload);
    s_server.onNotFound([]() {
        s_server.send(404, "text/plain", "404");
    });
    s_server.begin();

    /* OTA por rede a partir do Arduino IDE (porta de rede) */
    ArduinoOTA.setHostname(DEWVIEW_HOSTNAME);
    ArduinoOTA.setPassword(DEWVIEW_OTA_PASSWORD);
    ArduinoOTA
    .onStart([]() {
        s_ota_busy = true;
        Serial.println("OTA IDE: inicio");
    })
    .onEnd([]() {
        Serial.println("OTA IDE: concluido");
    })
    .onError([](ota_error_t error) {
        s_ota_busy = false;
        Serial.printf("OTA IDE: erro %u\n", error);
    });
    ArduinoOTA.begin();
}

void dewview_net_loop()
{
    s_server.handleClient();
    ArduinoOTA.handle();
}

void dewview_net_set_reading(const S24Reading &reading, bool ok)
{
    s_last_reading = reading;
    s_last_ok = ok;
}

bool dewview_net_ota_busy()
{
    return s_ota_busy;
}
