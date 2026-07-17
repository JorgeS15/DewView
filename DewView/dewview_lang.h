/**
 * @file dewview_lang.h
 * @brief Traducoes PT/EN da interface.
 *
 * A lingua ativa e comutada pelo botao no cabecalho do ecra e fica guardada
 * em NVS (sobrevive a reinicios). Os textos dinamicos usam dew_tr() no
 * momento em que sao construidos.
 */

#pragma once

struct DewStrings {
    /* Cartoes do painel */
    const char *temperature;
    const char *dew_point;
    const char *humidity;
    const char *margin;

    /* Separadores */
    const char *tab_panel;
    const char *tab_charts;
    const char *tab_system;

    /* Graficos */
    const char *legend_temp;
    const char *legend_dew;
    const char *rh_chart_title;

    /* Selo de risco */
    const char *safe;
    const char *warning;
    const char *risk;
    const char *no_data;

    /* Pagina Sistema */
    const char *card_system;
    const char *card_network;
    const char *card_modbus;
    const char *card_events;
    const char *no_events;
    const char *sys_fmt;      // args: build date, build time, uptime, heap, min heap, psram, w, h
    const char *net_fmt;      // args: AP ssid, AP ip, n clientes
    const char *net_fmt_sta;  // args: + estado STA, ip STA (modo TCP)
    const char *modbus_fmt;   // args: modo, leituras ok, falhas, ultimo erro

    /* Cabecalho / estados */
    const char *subtitle;
    const char *starting;
    const char *connecting_wifi;
    const char *sensor_ok_ap_fmt;   // arg: ip do AP
    const char *sensor_ok_sta_fmt;  // arg: ip STA (modo TCP)
    const char *scanning_fmt;       // args: baud, paridade
    const char *sensor_not_found;

    /* Registo de eventos */
    const char *log_boot;
    const char *log_recovered;
    const char *log_no_response_fmt;  // arg: erro
    const char *log_found_fmt;        // args: baud, paridade, addr
    const char *log_scan_start;
    const char *log_scan_failed;
};

/** Textos da lingua ativa. */
const DewStrings *dew_tr();

/** true se a lingua ativa for portugues. */
bool dew_lang_is_pt();

/** Comuta PT <-> EN e guarda a escolha em NVS. */
void dew_lang_toggle();

/** Carrega a lingua guardada em NVS (chamar uma vez no arranque). */
void dew_lang_load();
