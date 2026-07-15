/**
 * DewView - Dashboard de temperatura e ponto de orvalho
 *
 * Placa:  Waveshare ESP32-S3-Touch-LCD-5 (1024x600 ou 800x480)
 * Sensor: Banner S24 Dew Point (S24AS-D-MQP), Modbus RTU sobre RS-485
 *         (diretamente na porta RS485 da placa) ou via gateway Modbus TCP.
 *
 * Configuracao em `dewview_config.h`. Baseado no exemplo 09_lvgl_Porting.
 */

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"

#include "dewview_config.h"
#include "s24_modbus.h"
#include "dewview_ui.h"
#include "dewview_net.h"

#include <WiFi.h>

using namespace esp_panel::drivers;
using namespace esp_panel::board;

/* Numero de falhas consecutivas a partir do qual os valores passam a "--" */
static constexpr int STALE_AFTER_FAILURES = 3;

static S24Modbus s_sensor;
static uint32_t s_last_poll = 0;
static int s_failures = 0;          // falhas consecutivas
static uint32_t s_ok_count = 0;     // totais, para a pagina Sistema
static uint32_t s_fail_count = 0;

void setup()
{
    Serial.begin(115200);

    Serial.println("DewView v" DEWVIEW_VERSION " - " DEWVIEW_DEVELOPER);
    Serial.println("DewView: inicializar placa");
    Board *board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
    assert(board->begin());

    Serial.println("DewView: inicializar LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    lvgl_port_lock(-1);
    dewview_ui_create();
    dewview_ui_log("Arranque");
    lvgl_port_unlock();

    /* AP "DewView" + servidor web + OTA (e STA para o gateway no modo TCP) */
    dewview_net_begin();

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    lvgl_port_lock(-1);
    dewview_ui_set_status("A ligar WiFi...", false);
    lvgl_port_unlock();
#endif

    s_sensor.begin();
}

void loop()
{
    dewview_net_loop();

    /* Nao falar Modbus durante um upload de firmware */
    if (dewview_net_ota_busy()) {
        delay(10);
        return;
    }

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    if (WiFi.status() != WL_CONNECTED) {
        lvgl_port_lock(-1);
        dewview_ui_set_status("A ligar WiFi...", false);
        lvgl_port_unlock();
        delay(100);
        return;
    }
#endif

    if (millis() - s_last_poll < DEWVIEW_POLL_MS && s_last_poll != 0) {
        delay(10);
        return;
    }
    s_last_poll = millis();

    S24Reading reading;
    const bool ok = s_sensor.read(reading);
    dewview_net_set_reading(reading, ok);

    lvgl_port_lock(-1);
    if (ok) {
        if (s_failures >= STALE_AFTER_FAILURES) {
            dewview_ui_log("Sensor recuperado");
        }
        s_failures = 0;
        s_ok_count++;
        dewview_ui_update(reading);
#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
        static char status[48];
        snprintf(status, sizeof(status), "Sensor OK  |  %s", WiFi.localIP().toString().c_str());
        dewview_ui_set_status(status, true);
#else
        static char status[48];
        snprintf(status, sizeof(status), "Sensor OK  |  AP %s", WiFi.softAPIP().toString().c_str());
        dewview_ui_set_status(status, true);
#endif
        Serial.printf("T=%.1fC  Td=%.1fC  HR=%.1f%%\n",
                      reading.tempC, reading.dewC, reading.humidity);
    } else {
        s_failures++;
        s_fail_count++;
        dewview_ui_set_status(s_sensor.lastError(), false);
        if (s_failures == STALE_AFTER_FAILURES) {
            dewview_ui_set_stale();
            static char logmsg[64];
            snprintf(logmsg, sizeof(logmsg), "Sensor sem resposta (%s)", s_sensor.lastError());
            dewview_ui_log(logmsg);
        }
        Serial.printf("DewView: leitura falhou (%s) %s\n",
                      s_sensor.lastError(), s_sensor.lastErrorDetail());
    }
    static char err_full[160];
    if (s_sensor.lastErrorDetail()[0] != '\0') {
        snprintf(err_full, sizeof(err_full), "%s\n%s",
                 s_sensor.lastError(), s_sensor.lastErrorDetail());
    } else {
        snprintf(err_full, sizeof(err_full), "%s", s_sensor.lastError());
    }
    dewview_ui_diag_update(s_ok_count, s_fail_count, err_full);
    lvgl_port_unlock();
}
