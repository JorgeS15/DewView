#include <lvgl.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include "dewview_ui.h"
#include "dewview_config.h"
#include "dewview_lang.h"

/*================================ Paleta ==================================*/
/* Superficie escura; series validadas para CVD (laranja/azul). */
#define COL_BG          lv_color_hex(0x111110)
#define COL_SURFACE     lv_color_hex(0x1a1a19)
#define COL_TEXT        lv_color_hex(0xffffff)
#define COL_TEXT_2      lv_color_hex(0xc3c2b7)
#define COL_TEXT_MUTED  lv_color_hex(0x8a897e)
#define COL_GRID        lv_color_hex(0x2e2e2c)
#define COL_TEMP        lv_color_hex(0xd95926)   // serie: temperatura
#define COL_DEW         lv_color_hex(0x3987e5)   // serie: ponto de orvalho
#define COL_RH          lv_color_hex(0x199e70)   // serie/destaque: humidade
/* Estado (reservadas, sempre acompanhadas de simbolo + texto) */
#define COL_OK          lv_color_hex(0x2ea36e)
#define COL_WARN        lv_color_hex(0xc98500)
#define COL_CRIT        lv_color_hex(0xe66767)

/*
 * Fontes: o lv_conf.h deste repositorio ativa as Montserrat 20 e 48 para os
 * numeros grandes. Se estiveres a compilar com um lv_conf.h antigo (sem essas
 * fontes), usa-se o melhor substituto disponivel para nao partir a compilacao.
 */
#if LV_FONT_MONTSERRAT_48
#define DEW_FONT_VALUE  lv_font_montserrat_48
#else
#define DEW_FONT_VALUE  lv_font_montserrat_30
#endif
#if LV_FONT_MONTSERRAT_20
#define DEW_FONT_UNIT   lv_font_montserrat_20
#else
#define DEW_FONT_UNIT   lv_font_montserrat_16
#endif

/* Fonte grande (96 px, so digitos/simbolos) gerada em dewview_font_96.c */
LV_FONT_DECLARE(dewview_font_96);

#define HEADER_H        56
#define TABBAR_H        56
#define CHART_POINTS    600   // ~5 min de historico com poll de 500 ms (2 Hz)
#define LOG_MAX_CHARS   700   // registo de eventos da pagina Sistema

/* Valores no grafico em decimos (0.1 degC / 0.1 %HR) */
#define TO_TENTHS(v)    ((lv_coord_t)lroundf((v) * 10.0f))

/* Cabecalho */
static lv_obj_t *s_status_led;
static lv_obj_t *s_status_label;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_lang_btn_label;
static lv_obj_t *s_tabview;

/* Pagina 1: Painel */
static lv_obj_t *s_tile_titles[4];
static lv_obj_t *s_temp_value;
static lv_obj_t *s_dew_value;
static lv_obj_t *s_rh_value;
static lv_obj_t *s_margin_value;
static lv_obj_t *s_margin_badge;
static lv_obj_t *s_margin_badge_label;

/* Pagina 2: Graficos */
static lv_obj_t *s_legend_temp;
static lv_obj_t *s_legend_dew;
static lv_obj_t *s_rh_title;
static lv_obj_t *s_chart_td;
static lv_chart_series_t *s_ser_temp;
static lv_chart_series_t *s_ser_dew;
static lv_obj_t *s_chart_rh;
static lv_chart_series_t *s_ser_rh;

/* Pagina 3: Sistema */
static lv_obj_t *s_diag_titles[4];
static lv_obj_t *s_diag_sys;
static lv_obj_t *s_diag_net;
static lv_obj_t *s_diag_modbus;
static lv_obj_t *s_diag_log;
static char s_log_buf[LOG_MAX_CHARS] = "";

/*=============================== Utilitarios ==============================*/

/* lv_snprintf nao suporta floats por omissao; usa o snprintf da libc */
static void set_value_label(lv_obj_t *label, float value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", value);
    lv_label_set_text(label, buf);
}

static void format_uptime(char *buf, size_t len)
{
    const uint32_t s = millis() / 1000;
    snprintf(buf, len, "%lud %02lu:%02lu:%02lu",
             (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24),
             (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
}

/* Formata os ticks do eixo Y (valores internos em decimos) */
static void chart_draw_event_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_Y && dsc->text) {
        lv_snprintf(dsc->text, dsc->text_length, "%d", (int)lroundf(dsc->value / 10.0f));
    }
}

static lv_obj_t *make_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card, COL_SURFACE, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t *make_legend_entry(lv_obj_t *parent, const char *text, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_border_width(dot, 0, 0);

    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT_2, 0);
    return lbl;
}

/*============================ Pagina 1: Painel ============================*/

/* Cartao grande 2x2 com o valor em fonte dedicada, legivel a distancia. */
static lv_obj_t *make_big_tile(lv_obj_t *parent, const char *title, lv_color_t accent,
                               const char *unit, const lv_font_t *value_font,
                               lv_obj_t **value_out, lv_obj_t **title_out)
{
    lv_obj_t *card = make_card(parent);

    lv_obj_t *bar = lv_obj_create(card);
    lv_obj_set_size(bar, 6, LV_PCT(100));
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, -8, 0);
    lv_obj_set_style_bg_color(bar, accent, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 3, 0);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &DEW_FONT_UNIT, 0);
    lv_obj_set_style_text_color(label, COL_TEXT_2, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 8, 0);
    *title_out = label;

    lv_obj_t *unit_lbl = lv_label_create(card);
    lv_label_set_text(unit_lbl, unit);
    lv_obj_set_style_text_font(unit_lbl, &DEW_FONT_UNIT, 0);
    lv_obj_set_style_text_color(unit_lbl, COL_TEXT_MUTED, 0);
    lv_obj_align(unit_lbl, LV_ALIGN_TOP_RIGHT, -8, 0);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, value_font, 0);
    lv_obj_set_style_text_color(value, COL_TEXT, 0);
    /* centrado no espaco abaixo do titulo */
    lv_obj_align(value, LV_ALIGN_CENTER, 0, 18);

    *value_out = value;
    return card;
}

static void create_page_dashboard(lv_obj_t *page)
{
    lv_obj_set_style_pad_all(page, 12, 0);
    lv_obj_set_style_pad_gap(page, 12, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    const lv_coord_t tile_w = (LV_HOR_RES - 2 * 12 - 12) / 2;
    const lv_coord_t tile_h = (LV_VER_RES - HEADER_H - TABBAR_H - 2 * 12 - 12) / 2;

    const DewStrings *tr = dew_tr();
    lv_obj_t *t;
    t = make_big_tile(page, tr->temperature, COL_TEMP, "°C", &dewview_font_96,
                      &s_temp_value, &s_tile_titles[0]);
    lv_obj_set_size(t, tile_w, tile_h);
    t = make_big_tile(page, tr->dew_point, COL_DEW, "°C", &dewview_font_96,
                      &s_dew_value, &s_tile_titles[1]);
    lv_obj_set_size(t, tile_w, tile_h);
    t = make_big_tile(page, tr->humidity, COL_RH, "% HR", &dewview_font_96,
                      &s_rh_value, &s_tile_titles[2]);
    lv_obj_set_size(t, tile_w, tile_h);
    t = make_big_tile(page, tr->margin, COL_TEXT_MUTED, "°C", &dewview_font_96,
                      &s_margin_value, &s_tile_titles[3]);
    lv_obj_set_size(t, tile_w, tile_h);

    /* Selo de estado no cartao da margem */
    s_margin_badge = lv_obj_create(t);
    lv_obj_set_size(s_margin_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_margin_badge, 16, 0);
    lv_obj_set_style_pad_hor(s_margin_badge, 14, 0);
    lv_obj_set_style_pad_ver(s_margin_badge, 8, 0);
    lv_obj_set_style_border_width(s_margin_badge, 0, 0);
    lv_obj_set_style_bg_color(s_margin_badge, COL_WARN, 0);
    lv_obj_align(s_margin_badge, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_clear_flag(s_margin_badge, LV_OBJ_FLAG_SCROLLABLE);

    s_margin_badge_label = lv_label_create(s_margin_badge);
    lv_label_set_text(s_margin_badge_label, LV_SYMBOL_REFRESH " ...");
    lv_obj_set_style_text_font(s_margin_badge_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_margin_badge_label, lv_color_hex(0x111110), 0);
}

/*=========================== Pagina 2: Graficos ===========================*/

/*
 * As etiquetas do eixo Y sao desenhadas pelo LVGL a esquerda do gafico,
 * FORA do objeto (lv_chart draw_y_ticks usa coords.x1). O grafico e por isso
 * encolhido em `w` e alinhado a direita, deixando espaco para a escala
 * dentro do cartao (senao o cartao corta as etiquetas).
 */
static lv_obj_t *make_chart(lv_obj_t *card, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *chart = lv_chart_create(card);
    lv_obj_set_size(chart, w, h);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_line_color(chart, COL_GRID, LV_PART_MAIN);   // grelha recessiva
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(chart, COL_TEXT_MUTED, LV_PART_TICKS);
    lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);         // linhas finas
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);           // sem pontos

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(chart, 5, 7);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 5, 1, true, 40);
    lv_obj_add_event_cb(chart, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
    return chart;
}

static void create_page_charts(lv_obj_t *page)
{
    lv_obj_set_style_pad_all(page, 12, 0);
    lv_obj_set_style_pad_gap(page, 12, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    const lv_coord_t avail_h = LV_VER_RES - HEADER_H - TABBAR_H - 2 * 12 - 12;
    const lv_coord_t card_td_h = (avail_h * 3) / 5;
    const lv_coord_t card_rh_h = avail_h - card_td_h;
    /* largura do grafico: pagina - pad da pagina (2x12) - pad do cartao (2x12)
     * - 44 px reservados para as etiquetas do eixo Y */
    const lv_coord_t chart_w = LV_HOR_RES - 2 * 12 - 2 * 12 - 44;

    /* Temperatura + ponto de orvalho (mesmo eixo, degC) */
    lv_obj_t *card_td = make_card(page);
    lv_obj_set_size(card_td, LV_PCT(100), card_td_h);
    lv_obj_set_style_pad_all(card_td, 12, 0);
    lv_obj_align(card_td, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *legend = lv_obj_create(card_td);
    lv_obj_set_size(legend, LV_PCT(100), 24);
    lv_obj_align(legend, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_style_pad_column(legend, 8, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);

    s_legend_temp = make_legend_entry(legend, dew_tr()->legend_temp, COL_TEMP);
    lv_obj_t *spacer = lv_obj_create(legend);
    lv_obj_set_size(spacer, 12, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    s_legend_dew = make_legend_entry(legend, dew_tr()->legend_dew, COL_DEW);

    s_chart_td = make_chart(card_td, chart_w, card_td_h - 24 - 24 - 8);
    lv_chart_set_range(s_chart_td, LV_CHART_AXIS_PRIMARY_Y, TO_TENTHS(-10), TO_TENTHS(40));
    s_ser_temp = lv_chart_add_series(s_chart_td, COL_TEMP, LV_CHART_AXIS_PRIMARY_Y);
    s_ser_dew  = lv_chart_add_series(s_chart_td, COL_DEW, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_chart_td, s_ser_temp, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart_td, s_ser_dew, LV_CHART_POINT_NONE);

    /* Humidade (escala fixa 0-100 %HR) */
    lv_obj_t *card_rh = make_card(page);
    lv_obj_set_size(card_rh, LV_PCT(100), card_rh_h);
    lv_obj_set_style_pad_all(card_rh, 12, 0);
    lv_obj_align(card_rh, LV_ALIGN_TOP_MID, 0, card_td_h + 12);

    lv_obj_t *rh_title = lv_label_create(card_rh);
    s_rh_title = rh_title;
    lv_label_set_text(rh_title, dew_tr()->rh_chart_title);
    lv_obj_set_style_text_font(rh_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rh_title, COL_TEXT_2, 0);
    lv_obj_align(rh_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_chart_rh = make_chart(card_rh, chart_w, card_rh_h - 24 - 24 - 8);
    lv_chart_set_range(s_chart_rh, LV_CHART_AXIS_PRIMARY_Y, 0, TO_TENTHS(100));
    s_ser_rh = lv_chart_add_series(s_chart_rh, COL_RH, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_chart_rh, s_ser_rh, LV_CHART_POINT_NONE);
}

/* Ajusta a escala Y do grafico T/Td ao historico atual, com folga de 2 graus */
static void chart_td_autoscale()
{
    lv_coord_t *ta = lv_chart_get_y_array(s_chart_td, s_ser_temp);
    lv_coord_t *da = lv_chart_get_y_array(s_chart_td, s_ser_dew);
    lv_coord_t vmin = INT16_MAX, vmax = INT16_MIN;

    for (uint16_t i = 0; i < CHART_POINTS; i++) {
        if (ta[i] != LV_CHART_POINT_NONE) {
            vmin = LV_MIN(vmin, ta[i]);
            vmax = LV_MAX(vmax, ta[i]);
        }
        if (da[i] != LV_CHART_POINT_NONE) {
            vmin = LV_MIN(vmin, da[i]);
            vmax = LV_MAX(vmax, da[i]);
        }
    }
    if (vmin > vmax) {
        return;  // sem pontos validos
    }
    lv_chart_set_range(s_chart_td, LV_CHART_AXIS_PRIMARY_Y, vmin - 20, vmax + 20);
}

/*=========================== Pagina 3: Sistema ============================*/

static lv_obj_t *make_diag_card(lv_obj_t *parent, const char *title, lv_obj_t **body_out)
{
    lv_obj_t *card = make_card(parent);
    lv_obj_set_style_pad_all(card, 14, 0);

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, COL_TEXT_2, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *body = lv_label_create(card);
    lv_label_set_text(body, "...");
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(body, COL_TEXT, 0);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 26);

    *body_out = body;
    return card;
}

static void create_page_diag(lv_obj_t *page)
{
    lv_obj_set_style_pad_all(page, 12, 0);
    lv_obj_set_style_pad_gap(page, 12, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    const lv_coord_t tile_w = (LV_HOR_RES - 2 * 12 - 12) / 2;
    const lv_coord_t tile_h = (LV_VER_RES - HEADER_H - TABBAR_H - 2 * 12 - 12) / 2;

    const DewStrings *tr = dew_tr();
    lv_obj_t *c;
    c = make_diag_card(page, tr->card_system, &s_diag_sys);
    lv_obj_set_size(c, tile_w, tile_h);
    s_diag_titles[0] = lv_obj_get_child(c, 0);
    c = make_diag_card(page, tr->card_network, &s_diag_net);
    lv_obj_set_size(c, tile_w, tile_h);
    s_diag_titles[1] = lv_obj_get_child(c, 0);
    c = make_diag_card(page, tr->card_modbus, &s_diag_modbus);
    lv_obj_set_size(c, tile_w, tile_h);
    s_diag_titles[2] = lv_obj_get_child(c, 0);

    /* Registo de eventos (com scroll por toque) */
    c = make_diag_card(page, tr->card_events, &s_diag_log);
    lv_obj_set_size(c, tile_w, tile_h);
    s_diag_titles[3] = lv_obj_get_child(c, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(c, LV_DIR_VER);
    lv_label_set_text(s_diag_log, tr->no_events);
}

void dewview_ui_diag_update(uint32_t ok_count, uint32_t fail_count,
                            const char *last_error, const char *modbus_desc)
{
    char uptime[32];
    format_uptime(uptime, sizeof(uptime));

    const DewStrings *tr = dew_tr();
    char buf[320];
    snprintf(buf, sizeof(buf), tr->sys_fmt,
             __DATE__, __TIME__, uptime,
             (unsigned)(esp_get_free_heap_size() / 1024),
             (unsigned)(esp_get_minimum_free_heap_size() / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (int)LV_HOR_RES, (int)LV_VER_RES);
    lv_label_set_text(s_diag_sys, buf);

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    snprintf(buf, sizeof(buf), tr->net_fmt_sta,
             DEWVIEW_AP_SSID, WiFi.softAPIP().toString().c_str(),
             WiFi.softAPgetStationNum(),
             WiFi.status() == WL_CONNECTED ? "ON" : "OFF",
             WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-");
#else
    snprintf(buf, sizeof(buf), tr->net_fmt,
             DEWVIEW_AP_SSID, WiFi.softAPIP().toString().c_str(),
             WiFi.softAPgetStationNum());
#endif
    lv_label_set_text(s_diag_net, buf);

    snprintf(buf, sizeof(buf), tr->modbus_fmt,
             modbus_desc,
             (unsigned long)ok_count, (unsigned long)fail_count,
             (last_error && last_error[0]) ? last_error : "-");
    lv_label_set_text(s_diag_modbus, buf);
}

void dewview_ui_log(const char *msg)
{
    char uptime[32];
    format_uptime(uptime, sizeof(uptime));

    char line[96];
    const int n = snprintf(line, sizeof(line), "[%s] %s\n", uptime, msg);
    if (n <= 0) {
        return;
    }

    /* Prepend: linha nova primeiro, truncando o mais antigo */
    char merged[LOG_MAX_CHARS];
    snprintf(merged, sizeof(merged), "%s%s", line, s_log_buf);
    strncpy(s_log_buf, merged, sizeof(s_log_buf) - 1);
    s_log_buf[sizeof(s_log_buf) - 1] = '\0';
    lv_label_set_text(s_diag_log, s_log_buf);
}

/*============================== Lingua PT/EN ==============================*/

/* Reaplica os textos estaticos na lingua ativa (os dinamicos - estado,
 * selo, diagnostico - sao reconstruidos no ciclo de leitura seguinte). */
static void apply_language()
{
    const DewStrings *tr = dew_tr();

    lv_label_set_text(s_subtitle, tr->subtitle);
    lv_label_set_text(s_lang_btn_label, dew_lang_is_pt() ? "EN" : "PT");

    lv_label_set_text(s_tile_titles[0], tr->temperature);
    lv_label_set_text(s_tile_titles[1], tr->dew_point);
    lv_label_set_text(s_tile_titles[2], tr->humidity);
    lv_label_set_text(s_tile_titles[3], tr->margin);

    lv_label_set_text(s_legend_temp, tr->legend_temp);
    lv_label_set_text(s_legend_dew, tr->legend_dew);
    lv_label_set_text(s_rh_title, tr->rh_chart_title);

    lv_label_set_text(s_diag_titles[0], tr->card_system);
    lv_label_set_text(s_diag_titles[1], tr->card_network);
    lv_label_set_text(s_diag_titles[2], tr->card_modbus);
    lv_label_set_text(s_diag_titles[3], tr->card_events);

    char tab_name[40];
    snprintf(tab_name, sizeof(tab_name), LV_SYMBOL_HOME "  %s", tr->tab_panel);
    lv_tabview_rename_tab(s_tabview, 0, tab_name);
    snprintf(tab_name, sizeof(tab_name), LV_SYMBOL_IMAGE "  %s", tr->tab_charts);
    lv_tabview_rename_tab(s_tabview, 1, tab_name);
    snprintf(tab_name, sizeof(tab_name), LV_SYMBOL_SETTINGS "  %s", tr->tab_system);
    lv_tabview_rename_tab(s_tabview, 2, tab_name);
}

/* Corre no task do LVGL (ja com o mutex adquirido) */
static void lang_btn_event_cb(lv_event_t *e)
{
    (void)e;
    dew_lang_toggle();
    apply_language();
}

/*============================ Construcao geral ============================*/

void dewview_ui_create()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /*------------------------------ Cabecalho ----------------------------*/
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, LV_PCT(100), HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, COL_SURFACE, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_hor(header, 20, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "DewView");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    s_subtitle = lv_label_create(header);
    lv_label_set_text(s_subtitle, dew_tr()->subtitle);
    lv_obj_set_style_text_font(s_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_subtitle, COL_TEXT_MUTED, 0);
    lv_obj_align_to(s_subtitle, title, LV_ALIGN_OUT_RIGHT_MID, 16, 2);

    /* Botao PT/EN (mostra a lingua para a qual comuta) */
    lv_obj_t *lang_btn = lv_btn_create(header);
    lv_obj_set_size(lang_btn, 64, 36);
    lv_obj_align(lang_btn, LV_ALIGN_RIGHT_MID, -300, 0);
    lv_obj_set_style_bg_color(lang_btn, COL_BG, 0);
    lv_obj_set_style_radius(lang_btn, 8, 0);
    lv_obj_set_style_border_width(lang_btn, 1, 0);
    lv_obj_set_style_border_color(lang_btn, COL_TEXT_MUTED, 0);
    lv_obj_add_event_cb(lang_btn, lang_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_lang_btn_label = lv_label_create(lang_btn);
    lv_label_set_text(s_lang_btn_label, dew_lang_is_pt() ? "EN" : "PT");
    lv_obj_set_style_text_font(s_lang_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lang_btn_label, COL_TEXT_2, 0);
    lv_obj_center(s_lang_btn_label);

    s_status_led = lv_obj_create(header);
    lv_obj_set_size(s_status_led, 12, 12);
    lv_obj_set_style_radius(s_status_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_status_led, COL_WARN, 0);
    lv_obj_set_style_border_width(s_status_led, 0, 0);
    lv_obj_align(s_status_led, LV_ALIGN_RIGHT_MID, -260, 0);

    s_status_label = lv_label_create(header);
    lv_label_set_text(s_status_label, dew_tr()->starting);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, COL_TEXT_2, 0);
    lv_obj_align(s_status_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_label, 240);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_RIGHT, 0);

    /*-------------------------- Separadores (tabs) -----------------------*/
    lv_obj_t *tabview = lv_tabview_create(scr, LV_DIR_BOTTOM, TABBAR_H);
    s_tabview = tabview;
    lv_obj_set_size(tabview, LV_PCT(100), LV_VER_RES - HEADER_H);
    lv_obj_align(tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(tabview, COL_BG, 0);

    /* Estilo da barra de botoes */
    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, COL_SURFACE, 0);
    lv_obj_set_style_text_color(tab_btns, COL_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(tab_btns, COL_TEXT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, COL_DEW, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_btns, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP, LV_PART_ITEMS | LV_STATE_CHECKED);

    char tab_name[40];
    snprintf(tab_name, sizeof(tab_name), LV_SYMBOL_HOME "  %s", dew_tr()->tab_panel);
    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, tab_name);
    snprintf(tab_name, sizeof(tab_name), LV_SYMBOL_IMAGE "  %s", dew_tr()->tab_charts);
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, tab_name);
    snprintf(tab_name, sizeof(tab_name), LV_SYMBOL_SETTINGS "  %s", dew_tr()->tab_system);
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview, tab_name);

    create_page_dashboard(tab1);
    create_page_charts(tab2);
    create_page_diag(tab3);
}

/*============================= Atualizacoes ===============================*/

void dewview_ui_update(const S24Reading &reading)
{
    const float margin = reading.tempC - reading.dewC;

    set_value_label(s_temp_value, reading.tempC);
    set_value_label(s_dew_value, reading.dewC);
    set_value_label(s_rh_value, reading.humidity);
    set_value_label(s_margin_value, margin);

    char badge[40];
    if (margin >= DEWVIEW_MARGIN_OK) {
        lv_obj_set_style_bg_color(s_margin_badge, COL_OK, 0);
        snprintf(badge, sizeof(badge), LV_SYMBOL_OK " %s", dew_tr()->safe);
    } else if (margin >= DEWVIEW_MARGIN_WARN) {
        lv_obj_set_style_bg_color(s_margin_badge, COL_WARN, 0);
        snprintf(badge, sizeof(badge), LV_SYMBOL_WARNING " %s", dew_tr()->warning);
    } else {
        lv_obj_set_style_bg_color(s_margin_badge, COL_CRIT, 0);
        snprintf(badge, sizeof(badge), LV_SYMBOL_WARNING " %s", dew_tr()->risk);
    }
    lv_label_set_text(s_margin_badge_label, badge);

    lv_chart_set_next_value(s_chart_td, s_ser_temp, TO_TENTHS(reading.tempC));
    lv_chart_set_next_value(s_chart_td, s_ser_dew, TO_TENTHS(reading.dewC));
    lv_chart_set_next_value(s_chart_rh, s_ser_rh, TO_TENTHS(reading.humidity));
    chart_td_autoscale();
}

void dewview_ui_set_stale()
{
    lv_label_set_text(s_temp_value, "--");
    lv_label_set_text(s_dew_value, "--");
    lv_label_set_text(s_rh_value, "--");
    lv_label_set_text(s_margin_value, "--");
    lv_obj_set_style_bg_color(s_margin_badge, COL_WARN, 0);
    char badge[40];
    snprintf(badge, sizeof(badge), LV_SYMBOL_WARNING " %s", dew_tr()->no_data);
    lv_label_set_text(s_margin_badge_label, badge);
}

void dewview_ui_set_status(const char *text, bool ok)
{
    lv_label_set_text(s_status_label, text);
    lv_obj_set_style_bg_color(s_status_led, ok ? COL_OK : COL_CRIT, 0);
}
