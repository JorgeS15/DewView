#include <lvgl.h>
#include "dewview_ui.h"

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
#define COL_RH          lv_color_hex(0x199e70)   // destaque: humidade
/* Estado (reservadas, sempre acompanhadas de simbolo + texto) */
#define COL_OK          lv_color_hex(0x2ea36e)
#define COL_WARN        lv_color_hex(0xc98500)
#define COL_CRIT        lv_color_hex(0xe66767)

#define CHART_POINTS    120   // ~6 min de historico com poll de 3 s

/* Valores no grafico em decimos de grau para manter resolucao de 0.1 degC */
#define TO_TENTHS(v)    ((lv_coord_t)lroundf((v) * 10.0f))

static lv_obj_t *s_status_led;
static lv_obj_t *s_status_label;
static lv_obj_t *s_temp_value;
static lv_obj_t *s_dew_value;
static lv_obj_t *s_rh_value;
static lv_obj_t *s_margin_value;
static lv_obj_t *s_margin_badge;
static lv_obj_t *s_margin_badge_label;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_ser_temp;
static lv_chart_series_t *s_ser_dew;

/* lv_snprintf nao suporta floats por omissao; usa o snprintf da libc */
static void set_value_label(lv_obj_t *label, float value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", value);
    lv_label_set_text(label, buf);
}

/* Formata os ticks do eixo Y (valores internos em decimos de grau) */
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

/* Cartao de estatistica: barra de destaque, titulo, valor grande + unidade */
static lv_obj_t *make_stat_tile(lv_obj_t *parent, const char *title, lv_color_t accent,
                                const char *unit, lv_obj_t **value_out)
{
    lv_obj_t *card = make_card(parent);
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_height(card, LV_PCT(100));

    lv_obj_t *bar = lv_obj_create(card);
    lv_obj_set_size(bar, 6, LV_PCT(100));
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, -8, 0);
    lv_obj_set_style_bg_color(bar, accent, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 3, 0);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, COL_TEXT_2, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 8, 0);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(value, COL_TEXT, 0);
    lv_obj_align(value, LV_ALIGN_BOTTOM_LEFT, 8, -18);

    lv_obj_t *unit_lbl = lv_label_create(card);
    lv_label_set_text(unit_lbl, unit);
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(unit_lbl, COL_TEXT_MUTED, 0);
    lv_obj_align(unit_lbl, LV_ALIGN_BOTTOM_LEFT, 8, 4);

    *value_out = value;
    return card;
}

static void make_legend_entry(lv_obj_t *parent, const char *text, lv_color_t color)
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
}

void dewview_ui_create()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /*------------------------------ Cabecalho ----------------------------*/
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, LV_PCT(100), 56);
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

    lv_obj_t *subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, "Sensor S24  -  Ponto de Orvalho");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subtitle, COL_TEXT_MUTED, 0);
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_RIGHT_MID, 16, 2);

    s_status_led = lv_obj_create(header);
    lv_obj_set_size(s_status_led, 12, 12);
    lv_obj_set_style_radius(s_status_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_status_led, COL_WARN, 0);
    lv_obj_set_style_border_width(s_status_led, 0, 0);
    lv_obj_align(s_status_led, LV_ALIGN_RIGHT_MID, -260, 0);

    s_status_label = lv_label_create(header);
    lv_label_set_text(s_status_label, "A iniciar...");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, COL_TEXT_2, 0);
    lv_obj_align(s_status_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_label, 240);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_RIGHT, 0);

    /*--------------------------- Fila de cartoes -------------------------*/
    lv_obj_t *tiles = lv_obj_create(scr);
    lv_obj_set_size(tiles, LV_PCT(100), 170);
    lv_obj_align(tiles, LV_ALIGN_TOP_MID, 0, 56 + 12);
    lv_obj_set_style_bg_opa(tiles, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tiles, 0, 0);
    lv_obj_set_style_pad_hor(tiles, 16, 0);
    lv_obj_set_style_pad_ver(tiles, 0, 0);
    lv_obj_set_style_pad_column(tiles, 12, 0);
    lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(tiles, LV_OBJ_FLAG_SCROLLABLE);

    make_stat_tile(tiles, "Temperatura", COL_TEMP, "°C", &s_temp_value);
    make_stat_tile(tiles, "Ponto de Orvalho", COL_DEW, "°C", &s_dew_value);
    make_stat_tile(tiles, "Humidade", COL_RH, "% HR", &s_rh_value);

    /* Cartao da margem T - Td, com selo de estado (simbolo + texto) */
    lv_obj_t *margin_card = make_card(tiles);
    lv_obj_set_flex_grow(margin_card, 1);
    lv_obj_set_height(margin_card, LV_PCT(100));

    lv_obj_t *mlabel = lv_label_create(margin_card);
    lv_label_set_text(mlabel, "Margem T - Td");
    lv_obj_set_style_text_font(mlabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mlabel, COL_TEXT_2, 0);
    lv_obj_align(mlabel, LV_ALIGN_TOP_LEFT, 0, 0);

    s_margin_value = lv_label_create(margin_card);
    lv_label_set_text(s_margin_value, "--");
    lv_obj_set_style_text_font(s_margin_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_margin_value, COL_TEXT, 0);
    lv_obj_align(s_margin_value, LV_ALIGN_BOTTOM_LEFT, 0, -18);

    lv_obj_t *munit = lv_label_create(margin_card);
    lv_label_set_text(munit, "°C");
    lv_obj_set_style_text_font(munit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(munit, COL_TEXT_MUTED, 0);
    lv_obj_align(munit, LV_ALIGN_BOTTOM_LEFT, 0, 4);

    s_margin_badge = lv_obj_create(margin_card);
    lv_obj_set_size(s_margin_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_margin_badge, 14, 0);
    lv_obj_set_style_pad_hor(s_margin_badge, 12, 0);
    lv_obj_set_style_pad_ver(s_margin_badge, 6, 0);
    lv_obj_set_style_border_width(s_margin_badge, 0, 0);
    lv_obj_set_style_bg_color(s_margin_badge, COL_WARN, 0);
    lv_obj_align(s_margin_badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_clear_flag(s_margin_badge, LV_OBJ_FLAG_SCROLLABLE);

    s_margin_badge_label = lv_label_create(s_margin_badge);
    lv_label_set_text(s_margin_badge_label, LV_SYMBOL_REFRESH " ...");
    lv_obj_set_style_text_font(s_margin_badge_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_margin_badge_label, lv_color_hex(0x111110), 0);

    /*------------------------------- Grafico -----------------------------*/
    const lv_coord_t chart_card_h = LV_VER_RES - (56 + 12 + 170 + 12 + 16);
    lv_obj_t *chart_card = make_card(scr);
    lv_obj_set_size(chart_card, LV_HOR_RES - 32, chart_card_h);  // margem lateral igual a das tiles
    lv_obj_align(chart_card, LV_ALIGN_TOP_MID, 0, 56 + 12 + 170 + 12);
    lv_obj_set_style_pad_all(chart_card, 12, 0);

    lv_obj_t *legend = lv_obj_create(chart_card);
    lv_obj_set_size(legend, LV_PCT(100), 24);
    lv_obj_align(legend, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_style_pad_column(legend, 8, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);

    make_legend_entry(legend, "Temperatura", COL_TEMP);
    lv_obj_t *spacer = lv_obj_create(legend);
    lv_obj_set_size(spacer, 12, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    make_legend_entry(legend, "Ponto de orvalho", COL_DEW);

    s_chart = lv_chart_create(chart_card);
    /* altura do cartao menos padding (2x12), legenda (24) e folga (8) */
    lv_obj_set_size(s_chart, LV_PCT(100), chart_card_h - 24 - 24 - 8);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_left(s_chart, 44, 0);   // espaco para os ticks do eixo Y
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_line_color(s_chart, COL_GRID, LV_PART_MAIN);   // grelha recessiva
    lv_obj_set_style_line_width(s_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_chart, COL_TEXT_MUTED, LV_PART_TICKS);
    lv_obj_set_style_text_font(s_chart, &lv_font_montserrat_12, LV_PART_TICKS);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);         // linhas finas
    lv_obj_set_style_size(s_chart, 0, LV_PART_INDICATOR);           // sem pontos

    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, CHART_POINTS);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(s_chart, 5, 7);
    lv_chart_set_axis_tick(s_chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 5, 1, true, 40);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, TO_TENTHS(-10), TO_TENTHS(40));
    lv_obj_add_event_cb(s_chart, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    s_ser_temp = lv_chart_add_series(s_chart, COL_TEMP, LV_CHART_AXIS_PRIMARY_Y);
    s_ser_dew  = lv_chart_add_series(s_chart, COL_DEW, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_chart, s_ser_temp, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart, s_ser_dew, LV_CHART_POINT_NONE);
}

/* Ajusta a escala Y ao historico atual, com folga de 2 graus */
static void chart_autoscale()
{
    lv_coord_t *ta = lv_chart_get_y_array(s_chart, s_ser_temp);
    lv_coord_t *da = lv_chart_get_y_array(s_chart, s_ser_dew);
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
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, vmin - 20, vmax + 20);
}

void dewview_ui_update(const S24Reading &reading)
{
    const float margin = reading.tempC - reading.dewC;

    set_value_label(s_temp_value, reading.tempC);
    set_value_label(s_dew_value, reading.dewC);
    set_value_label(s_rh_value, reading.humidity);
    set_value_label(s_margin_value, margin);

    if (margin >= DEWVIEW_MARGIN_OK) {
        lv_obj_set_style_bg_color(s_margin_badge, COL_OK, 0);
        lv_label_set_text(s_margin_badge_label, LV_SYMBOL_OK " Seguro");
    } else if (margin >= DEWVIEW_MARGIN_WARN) {
        lv_obj_set_style_bg_color(s_margin_badge, COL_WARN, 0);
        lv_label_set_text(s_margin_badge_label, LV_SYMBOL_WARNING " Alerta");
    } else {
        lv_obj_set_style_bg_color(s_margin_badge, COL_CRIT, 0);
        lv_label_set_text(s_margin_badge_label, LV_SYMBOL_WARNING " Risco");
    }

    lv_chart_set_next_value(s_chart, s_ser_temp, TO_TENTHS(reading.tempC));
    lv_chart_set_next_value(s_chart, s_ser_dew, TO_TENTHS(reading.dewC));
    chart_autoscale();
}

void dewview_ui_set_stale()
{
    lv_label_set_text(s_temp_value, "--");
    lv_label_set_text(s_dew_value, "--");
    lv_label_set_text(s_rh_value, "--");
    lv_label_set_text(s_margin_value, "--");
    lv_obj_set_style_bg_color(s_margin_badge, COL_WARN, 0);
    lv_label_set_text(s_margin_badge_label, LV_SYMBOL_WARNING " Sem dados");
}

void dewview_ui_set_status(const char *text, bool ok)
{
    lv_label_set_text(s_status_label, text);
    lv_obj_set_style_bg_color(s_status_led, ok ? COL_OK : COL_CRIT, 0);
}
