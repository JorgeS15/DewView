/**
 * @file dewview_ui.h
 * @brief Dashboard LVGL do DewView (1024x600 / 800x480).
 *
 * Todas as funcoes devem ser chamadas com o mutex do LVGL adquirido
 * (lvgl_port_lock / lvgl_port_unlock).
 */

#pragma once

#include "s24_modbus.h"

/** Cria o ecra do dashboard. Chamar uma vez apos lvgl_port_init(). */
void dewview_ui_create();

/** Atualiza os cartoes e o grafico com uma leitura valida do sensor. */
void dewview_ui_update(const S24Reading &reading);

/** Mostra "--" nos valores (sem dados validos do sensor). */
void dewview_ui_set_stale();

/** Atualiza o texto e o LED de estado no cabecalho. */
void dewview_ui_set_status(const char *text, bool ok);
