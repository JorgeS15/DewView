/**
 * @file dewview_net.h
 * @brief Rede e OTA do DewView: Access Point, servidor web e atualizacao
 *        de firmware (pagina /update e ArduinoOTA).
 *
 * A placa cria sempre o AP DEWVIEW_AP_SSID. No modo Modbus TCP funciona em
 * AP+STA: alem do AP, liga-se a rede do gateway (DEWVIEW_WIFI_SSID).
 */

#pragma once

#include "s24_modbus.h"

/** Arranca o WiFi (AP ou AP+STA), o servidor web, o mDNS e o ArduinoOTA. */
void dewview_net_begin();

/** Processa pedidos web/OTA; chamar em cada iteracao do loop(). */
void dewview_net_loop();

/** Guarda a ultima leitura para mostrar na pagina web de estado. */
void dewview_net_set_reading(const S24Reading &reading, bool ok);

/** true enquanto um upload de firmware estiver em curso (pausa o Modbus). */
bool dewview_net_ota_busy();
