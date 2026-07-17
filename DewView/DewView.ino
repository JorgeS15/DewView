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
#include "dewview_lang.h"

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

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_RTU
/*
 * Auto-scan: quando o sensor nao responde com os parametros configurados,
 * percorre baud x paridade x endereco (1..16) ate o encontrar. Um probe por
 * iteracao do loop() para manter a UI, o AP e o OTA a responder.
 */
static constexpr int SCAN_AFTER_FAILURES = 4;
static constexpr uint16_t SCAN_PROBE_TIMEOUT_MS = 120;
static constexpr uint32_t SCAN_RETRY_MS = 30000;
static const uint32_t SCAN_BAUDS[] = {19200, 9600, 38400};
static const uint32_t SCAN_SERIAL_CONFIGS[] = {SERIAL_8N1, SERIAL_8E1, SERIAL_8O1};
static const char *SCAN_CONFIG_NAMES[] = {"8N1", "8E1", "8O1"};
static constexpr uint8_t SCAN_MAX_ADDR = 16;
static constexpr size_t SCAN_TOTAL = 3 * 3 * SCAN_MAX_ADDR;
static bool s_scan_active = false;
static size_t s_scan_idx = 0;
static uint32_t s_scan_retry_at = 0;

/* Faz um probe do scan; devolve true se o sensor foi encontrado. */
static bool scan_step()
{
    const size_t combo = s_scan_idx / SCAN_MAX_ADDR;      // 0..8
    const uint8_t addr = (s_scan_idx % SCAN_MAX_ADDR) + 1;
    const uint32_t baud = SCAN_BAUDS[combo % 3];
    const size_t cfg_i = combo / 3;

    if (addr == 1) {  // novo grupo baud/paridade: atualiza o estado no ecra
        static char status[48];
        snprintf(status, sizeof(status), dew_tr()->scanning_fmt,
                 (unsigned long)baud, SCAN_CONFIG_NAMES[cfg_i]);
        lvgl_port_lock(-1);
        dewview_ui_set_status(status, false);
        lvgl_port_unlock();
    }

    if (s_sensor.probe(baud, SCAN_SERIAL_CONFIGS[cfg_i], addr, SCAN_PROBE_TIMEOUT_MS)) {
        static char msg[64];
        snprintf(msg, sizeof(msg), dew_tr()->log_found_fmt,
                 (unsigned long)baud, SCAN_CONFIG_NAMES[cfg_i], addr);
        Serial.println(msg);
        lvgl_port_lock(-1);
        dewview_ui_log(msg);
        lvgl_port_unlock();
        return true;
    }
    return false;
}
#endif  // DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_RTU

void setup()
{
    Serial.begin(115200);

    Serial.println("DewView v" DEWVIEW_VERSION " - " DEWVIEW_DEVELOPER);
    dew_lang_load();
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
    dewview_ui_log(dew_tr()->log_boot);
    lvgl_port_unlock();

    /* AP "DewView" + servidor web + OTA (e STA para o gateway no modo TCP) */
    dewview_net_begin();

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    lvgl_port_lock(-1);
    dewview_ui_set_status(dew_tr()->connecting_wifi, false);
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
        dewview_ui_set_status(dew_tr()->connecting_wifi, false);
        lvgl_port_unlock();
        delay(100);
        return;
    }
#else
    /* Auto-scan de baud/paridade/endereco quando o sensor nao responde */
    if (s_scan_active) {
        if (scan_step()) {
            s_scan_active = false;
            s_failures = 0;
            s_last_poll = 0;  // le ja com os parametros encontrados
        } else if (++s_scan_idx >= SCAN_TOTAL) {
            s_scan_active = false;
            s_scan_retry_at = millis() + SCAN_RETRY_MS;
            s_sensor.setParams(DEWVIEW_RS485_BAUD, DEWVIEW_RS485_CONFIG,
                               DEWVIEW_MODBUS_UNIT_ID);
            lvgl_port_lock(-1);
            dewview_ui_log(dew_tr()->log_scan_failed);
            dewview_ui_set_status(dew_tr()->sensor_not_found, false);
            lvgl_port_unlock();
        }
        return;
    }
    if (s_failures >= SCAN_AFTER_FAILURES && (int32_t)(millis() - s_scan_retry_at) > 0) {
        s_scan_active = true;
        s_scan_idx = 0;
        lvgl_port_lock(-1);
        dewview_ui_log(dew_tr()->log_scan_start);
        lvgl_port_unlock();
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
            dewview_ui_log(dew_tr()->log_recovered);
        }
        s_failures = 0;
        s_ok_count++;
        dewview_ui_update(reading);
        static char status[48];
#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
        snprintf(status, sizeof(status), dew_tr()->sensor_ok_sta_fmt,
                 WiFi.localIP().toString().c_str());
#else
        snprintf(status, sizeof(status), dew_tr()->sensor_ok_ap_fmt,
                 WiFi.softAPIP().toString().c_str());
#endif
        dewview_ui_set_status(status, true);
        Serial.printf("T=%.1fC  Td=%.1fC  HR=%.1f%%\n",
                      reading.tempC, reading.dewC, reading.humidity);
    } else {
        s_failures++;
        s_fail_count++;
        dewview_ui_set_status(s_sensor.lastError(), false);
        if (s_failures == STALE_AFTER_FAILURES) {
            dewview_ui_set_stale();
            static char logmsg[64];
            snprintf(logmsg, sizeof(logmsg), dew_tr()->log_no_response_fmt, s_sensor.lastError());
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
    static char modbus_desc[64];
#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    snprintf(modbus_desc, sizeof(modbus_desc), "TCP %s:%d (unit %d)",
             DEWVIEW_TCP_HOST, DEWVIEW_TCP_PORT, DEWVIEW_MODBUS_UNIT_ID);
#else
    snprintf(modbus_desc, sizeof(modbus_desc), "RTU RS485 %lu %s (addr %u)",
             (unsigned long)s_sensor.activeBaud(), s_sensor.activeParity(),
             s_sensor.activeAddr());
#endif
    dewview_ui_diag_update(s_ok_count, s_fail_count, err_full, modbus_desc);
    lvgl_port_unlock();
}
