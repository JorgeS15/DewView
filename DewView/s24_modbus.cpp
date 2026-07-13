#include "s24_modbus.h"

/* Registos do S24: 40001..40004 -> enderecos 0..3 no protocolo */
static constexpr uint16_t S24_REG_START = 0;
static constexpr uint16_t S24_REG_COUNT = 4;
static constexpr uint16_t S24_IDX_RH    = 0;   // 40001
static constexpr uint16_t S24_IDX_TEMPC = 1;   // 40002
static constexpr uint16_t S24_IDX_DEWC  = 3;   // 40004

bool S24Modbus::read(S24Reading &out)
{
    uint16_t regs[S24_REG_COUNT] = {0};

    out.valid = false;
    if (!readHoldingRegisters(S24_REG_START, S24_REG_COUNT, regs)) {
        return false;
    }

    out.humidity = regs[S24_IDX_RH] / DEWVIEW_SCALE_RH;
    out.tempC    = (int16_t)regs[S24_IDX_TEMPC] / DEWVIEW_SCALE_TEMP;
    out.dewC     = (int16_t)regs[S24_IDX_DEWC] / DEWVIEW_SCALE_DEW;
    out.valid    = true;
    return true;
}

#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP

/*============================== Modbus TCP ================================*/

void S24Modbus::begin()
{
    // A ligacao TCP e aberta (e reaberta) on-demand em ensureConnected().
}

bool S24Modbus::ensureConnected()
{
    if (_client.connected()) {
        return true;
    }
    _client.stop();
    if (!_client.connect(DEWVIEW_TCP_HOST, DEWVIEW_TCP_PORT, DEWVIEW_TIMEOUT_MS)) {
        _error = "gateway TCP inacessivel";
        return false;
    }
    _client.setTimeout(DEWVIEW_TIMEOUT_MS);
    return true;
}

bool S24Modbus::readHoldingRegisters(uint16_t startAddr, uint16_t count, uint16_t *regs)
{
    if (!ensureConnected()) {
        return false;
    }

    // Descarta bytes pendentes de transacoes anteriores
    while (_client.available()) {
        _client.read();
    }

    _txId++;
    const uint8_t request[12] = {
        (uint8_t)(_txId >> 8), (uint8_t)_txId,      // transaction id
        0x00, 0x00,                                 // protocol id
        0x00, 0x06,                                 // length
        DEWVIEW_MODBUS_UNIT_ID,                     // unit id
        0x03,                                       // funcao: read holding registers
        (uint8_t)(startAddr >> 8), (uint8_t)startAddr,
        (uint8_t)(count >> 8), (uint8_t)count,
    };
    if (_client.write(request, sizeof(request)) != sizeof(request)) {
        _error = "falha no envio TCP";
        _client.stop();
        return false;
    }

    // Resposta: MBAP (7) + funcao (1) + byte count (1) + dados (2*count)
    const size_t expected = 9 + 2 * count;
    uint8_t resp[9 + 2 * S24_REG_COUNT];
    size_t got = 0;
    const uint32_t deadline = millis() + DEWVIEW_TIMEOUT_MS;
    while (got < expected && (int32_t)(deadline - millis()) > 0) {
        int c = _client.read();
        if (c < 0) {
            delay(2);
            continue;
        }
        resp[got++] = (uint8_t)c;
    }

    if (got < 9) {
        _error = "sem resposta do gateway";
        _client.stop();
        return false;
    }
    if (resp[7] == 0x83) {  // excecao Modbus
        _error = "excecao Modbus do sensor";
        return false;
    }
    if (got < expected || resp[7] != 0x03 || resp[8] != 2 * count) {
        _error = "resposta TCP invalida";
        _client.stop();
        return false;
    }

    for (uint16_t i = 0; i < count; i++) {
        regs[i] = ((uint16_t)resp[9 + 2 * i] << 8) | resp[10 + 2 * i];
    }
    _error = "";
    return true;
}

#else

/*========================= Modbus RTU (RS-485) ============================*/

#define RS485 Serial1

void S24Modbus::begin()
{
    RS485.begin(DEWVIEW_RS485_BAUD, DEWVIEW_RS485_CONFIG,
                DEWVIEW_RS485_RX_PIN, DEWVIEW_RS485_TX_PIN);
}

uint16_t S24Modbus::crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

bool S24Modbus::readHoldingRegisters(uint16_t startAddr, uint16_t count, uint16_t *regs)
{
    while (RS485.available()) {
        RS485.read();
    }

    uint8_t request[8] = {
        DEWVIEW_MODBUS_UNIT_ID,
        0x03,
        (uint8_t)(startAddr >> 8), (uint8_t)startAddr,
        (uint8_t)(count >> 8), (uint8_t)count,
        0, 0,
    };
    const uint16_t crc = crc16(request, 6);
    request[6] = (uint8_t)crc;          // CRC: byte baixo primeiro
    request[7] = (uint8_t)(crc >> 8);

    RS485.write(request, sizeof(request));
    RS485.flush();  // espera fim da transmissao (transceiver com direcao automatica)

    // Resposta: endereco (1) + funcao (1) + byte count (1) + dados (2*count) + CRC (2)
    const size_t expected = 5 + 2 * count;
    uint8_t resp[5 + 2 * S24_REG_COUNT];
    size_t got = 0;
    const uint32_t deadline = millis() + DEWVIEW_TIMEOUT_MS;
    while (got < expected && (int32_t)(deadline - millis()) > 0) {
        if (RS485.available()) {
            resp[got++] = (uint8_t)RS485.read();
            // Excecao Modbus: trama curta (endereco + 0x83 + codigo + CRC)
            if (got == 5 && resp[1] == 0x83) {
                _error = "excecao Modbus do sensor";
                return false;
            }
        } else {
            delay(2);
        }
    }

    if (got < expected) {
        _error = "sem resposta RS485";
        return false;
    }
    if (resp[0] != DEWVIEW_MODBUS_UNIT_ID || resp[1] != 0x03 || resp[2] != 2 * count) {
        _error = "resposta RTU invalida";
        return false;
    }
    const uint16_t rxCrc = (uint16_t)resp[expected - 1] << 8 | resp[expected - 2];
    if (rxCrc != crc16(resp, expected - 2)) {
        _error = "erro de CRC";
        return false;
    }

    for (uint16_t i = 0; i < count; i++) {
        regs[i] = ((uint16_t)resp[3 + 2 * i] << 8) | resp[4 + 2 * i];
    }
    _error = "";
    return true;
}

#endif  // DEWVIEW_MODBUS_MODE
