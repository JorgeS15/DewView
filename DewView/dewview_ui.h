/**
 * @file dewview_ui.h
 * @brief Interface LVGL do DewView (1024x600, touch), com 3 paginas:
 *        Painel (valores grandes), Graficos (historico) e Sistema (diagnostico).
 *
 * Todas as funcoes devem ser chamadas com o mutex do LVGL adquirido
 * (lvgl_port_lock / lvgl_port_unlock).
 */

#pragma once

#include "s24_modbus.h"

/** Cria o ecra (cabecalho + separadores). Chamar uma vez apos lvgl_port_init(). */
void dewview_ui_create();

/** Atualiza o painel e os graficos com uma leitura valida do sensor. */
void dewview_ui_update(const S24Reading &reading);

/** Mostra "--" nos valores (sem dados validos do sensor). */
void dewview_ui_set_stale();

/** Atualiza o texto e o LED de estado no cabecalho. */
void dewview_ui_set_status(const char *text, bool ok);

/**
 * Atualiza a pagina Sistema: contadores Modbus, memoria, rede e uptime.
 * Chamar periodicamente (ex.: em cada ciclo de leitura).
 */
void dewview_ui_diag_update(uint32_t ok_count, uint32_t fail_count, const char *last_error);

/** Acrescenta uma linha (com uptime) ao registo de eventos da pagina Sistema. */
void dewview_ui_log(const char *msg);
