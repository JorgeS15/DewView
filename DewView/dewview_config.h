/**
 * @file dewview_config.h
 * @brief Configuracao do utilizador para o DewView (dashboard do sensor S24).
 *
 * Edita este ficheiro antes de compilar: modo Modbus, credenciais WiFi,
 * endereco do gateway, etc.
 */

#pragma once

/*======================== Modo de ligacao ao sensor =======================*/

#define DEWVIEW_MODBUS_TCP  0   // S24 ligado a um gateway RS485 -> Modbus TCP (via WiFi)
#define DEWVIEW_MODBUS_RTU  1   // S24 ligado diretamente a porta RS485 da placa

/**
 * Escolhe o modo:
 *  - DEWVIEW_MODBUS_TCP: precisa de WiFi + gateway Modbus TCP (ex.: USR-DR302,
 *    Moxa MGate). Configura o IP/porta abaixo.
 *  - DEWVIEW_MODBUS_RTU: o S24 e um sensor RS-485 nativo; liga-o diretamente
 *    ao terminal RS485 da ESP32-S3-Touch-LCD-5 (A/B) e nao precisas de rede.
 */
#define DEWVIEW_MODBUS_MODE DEWVIEW_MODBUS_TCP

/*========================= Access Point + OTA =============================*/

/**
 * A placa cria sempre esta rede WiFi ao arrancar. Liga-te a ela para ver o
 * estado (http://192.168.4.1 ou http://dewview.local) e atualizar o firmware
 * (http://192.168.4.1/update, ou porta de rede do Arduino IDE).
 */
#define DEWVIEW_AP_SSID         "DewView"
#define DEWVIEW_AP_PASSWORD     "Orvalho88"
#define DEWVIEW_HOSTNAME        "dewview"

/* Credenciais pedidas pela pagina /update e pelo OTA do Arduino IDE */
#define DEWVIEW_OTA_USER        "admin"
#define DEWVIEW_OTA_PASSWORD    "Orvalho88"

/*============================== WiFi (modo TCP) ===========================*/

/* Rede existente onde esta o gateway Modbus TCP (so usado no modo TCP) */
#define DEWVIEW_WIFI_SSID       "O_TEU_SSID"
#define DEWVIEW_WIFI_PASSWORD   "A_TUA_PASSWORD"

/*========================= Gateway Modbus TCP =============================*/

#define DEWVIEW_TCP_HOST        "192.168.1.50"  // IP do gateway Modbus TCP
#define DEWVIEW_TCP_PORT        502             // Porta Modbus TCP (standard: 502)

/*===================== RS-485 / Modbus RTU (modo RTU) =====================*/

/**
 * Pinos RS485 da ESP32-S3-Touch-LCD-5 (ver exemplo 02_RS485_Test).
 * O transceiver da placa tem controlo de direcao automatico.
 *
 * NOTA: os GPIO 43/44 sao partilhados com o UART0. Em modo RTU ativa
 * "USB CDC On Boot" no Arduino IDE para continuar a ter monitor serie por USB.
 */
#define DEWVIEW_RS485_RX_PIN    43
#define DEWVIEW_RS485_TX_PIN    44

/**
 * Parametros de fabrica do S24 (registos 40601/40602): 19200 baud, sem paridade.
 */
#define DEWVIEW_RS485_BAUD      19200
#define DEWVIEW_RS485_CONFIG    SERIAL_8N1

/*============================ Parametros Modbus ===========================*/

#define DEWVIEW_MODBUS_UNIT_ID  1       // Endereco Modbus do S24 (fabrica: 1)
#define DEWVIEW_POLL_MS         3000    // Intervalo de leitura (o S24 amostra a cada 3 s)
#define DEWVIEW_TIMEOUT_MS      1000    // Timeout de resposta Modbus

/*====================== Escalas dos registos do S24 =======================*/

/**
 * Manual do S24 (p/n 235396 Rev. B):
 *   40001 Humidade (%RH)      = registo / 100
 *   40002 Temperatura (degC)  = registo / 20
 *   40004 Ponto orvalho (degC)= registo / 100
 *
 * Atencao: a tabela do manual sugere /20 tambem para o ponto de orvalho, mas a
 * nota de rodape diz /100. Se a leitura do ponto de orvalho parecer errada por
 * um fator de 5, troca DEWVIEW_SCALE_DEW para 20.0f.
 */
#define DEWVIEW_SCALE_RH        100.0f
#define DEWVIEW_SCALE_TEMP      20.0f
#define DEWVIEW_SCALE_DEW       100.0f

/*========================= Limiares de margem T-Td ========================*/

/**
 * Margem entre a temperatura e o ponto de orvalho (risco de condensacao):
 *   margem >= DEWVIEW_MARGIN_OK   -> "Seguro"
 *   margem >= DEWVIEW_MARGIN_WARN -> "Alerta"
 *   abaixo                        -> "Risco"
 */
#define DEWVIEW_MARGIN_OK       10.0f
#define DEWVIEW_MARGIN_WARN     5.0f
