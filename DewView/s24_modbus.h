/**
 * @file s24_modbus.h
 * @brief Cliente Modbus (TCP ou RTU) para o sensor Banner S24 Dew Point.
 *
 * Le os registos 40001..40004 (funcao 0x03, holding registers):
 *   40001 Humidade relativa, 40002 Temperatura degC, 40004 Ponto de orvalho degC.
 */

#pragma once

#include <Arduino.h>
#include "dewview_config.h"

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
#include <WiFi.h>
#endif

struct S24Reading {
    float humidity;     // %RH
    float tempC;        // degC
    float dewC;         // degC
    bool valid;
};

class S24Modbus {
public:
    void begin();

    /**
     * Le o sensor (bloqueante, com timeout). Devolve true e preenche `out`
     * em caso de sucesso; false caso contrario (ver lastError()).
     */
    bool read(S24Reading &out);

    /** Mensagem curta do ultimo erro (ex.: "sem resposta RS485"). */
    const char *lastError() const { return _error; }

    /**
     * Detalhe tecnico do ultimo erro: numero de bytes recebidos e dump
     * hexadecimal da resposta, para diagnostico de cablagem/baud/endereco.
     * String vazia se a ultima leitura foi bem sucedida.
     */
    const char *lastErrorDetail() const { return _detail; }

private:
    bool readHoldingRegisters(uint16_t startAddr, uint16_t count, uint16_t *regs);
    void setDetail(const char *prefix, const uint8_t *data, size_t len);

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    bool ensureConnected();
    WiFiClient _client;
    uint16_t _txId = 0;
#else
    static uint16_t crc16(const uint8_t *data, size_t len);
#endif

    const char *_error = "";
    char _detail[96] = "";
};
