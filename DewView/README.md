# DewView — Dashboard de Ponto de Orvalho

Dashboard para o módulo **Waveshare ESP32-S3-Touch-LCD-5** (1024×600 ou 800×480)
que mostra em tempo real a **temperatura**, o **ponto de orvalho** e a **humidade**
lidos de um sensor **Banner S24 Dew Point (S24AS-D-MQP)** por Modbus.

O ecrã apresenta:

- 4 cartões: Temperatura, Ponto de Orvalho, Humidade e **Margem T−Td** (risco de
  condensação, com selo Seguro / Alerta / Risco);
- Gráfico com o histórico das duas séries (~6 min com leituras a cada 3 s);
- Barra de estado com a ligação WiFi/Modbus.

## Modos de ligação ao sensor

O S24 é um sensor **Modbus RTU sobre RS-485** nativo. Há duas formas de o ligar:

| Modo | Como | Configuração |
|------|------|--------------|
| **RTU (recomendado)** | Sensor ligado diretamente ao terminal RS485 (A/B) da placa. Não precisa de rede. | `DEWVIEW_MODBUS_MODE DEWVIEW_MODBUS_RTU` |
| **TCP** | Sensor ligado a um gateway RS485→Modbus TCP (ex.: USR-DR302, Moxa MGate); a placa liga-se por WiFi. | `DEWVIEW_MODBUS_MODE DEWVIEW_MODBUS_TCP` (predefinido) |

### Ligações do S24 (modo RTU)

Conector M12 de 4 pinos do S24 → terminal RS485 da placa + fonte externa:

| Pino S24 | Cor | Ligação |
|----------|-----|---------|
| 1 | Castanho | +10…30 V DC (fonte externa, ex.: 24 V) |
| 2 | Branco | RS485 **B** (D1/+) |
| 3 | Azul | GND (comum com a fonte e com a placa) |
| 4 | Preto | RS485 **A** (D0/−) |

Parâmetros de fábrica do S24: endereço **1**, **19200** baud, sem paridade (8N1).

## Registos Modbus usados (manual p/n 235396 Rev. B)

| Registo | Conteúdo | Escala |
|---------|----------|--------|
| 40001 | Humidade (%RH) | valor ÷ 100 |
| 40002 | Temperatura (°C) | valor ÷ 20 |
| 40004 | Ponto de orvalho (°C) | valor ÷ 100 |

> Nota: a tabela do manual sugere ÷20 também para o ponto de orvalho, mas a nota
> de rodapé diz ÷100. Se a leitura parecer errada por um fator de 5, ajusta
> `DEWVIEW_SCALE_DEW` em `dewview_config.h`.

## Como compilar (Arduino IDE)

1. Instala o suporte **esp32 by Espressif** (Boards Manager) — versão 3.x.
2. Copia as bibliotecas de `ESP32-S3-Touch-LCD-5-Demo/Arduino/libraries/`
   (`ESP32_Display_Panel`, `ESP32_IO_Expander`, `esp-lib-utils`, `lvgl` e o
   ficheiro `lv_conf.h`) para a pasta `libraries` do teu sketchbook Arduino.
   O `lv_conf.h` deste repositório já tem as fontes Montserrat 20 e 48 ativas
   (necessárias para este projeto).
3. Abre `DewView/DewView.ino` e edita `dewview_config.h`:
   - modo Modbus (RTU ou TCP);
   - SSID/password WiFi e IP do gateway (só no modo TCP);
   - endereço Modbus do sensor, se o tiveres alterado.
4. Em `esp_panel_board_custom_conf.h` confirma `ESP_PANEL_USE_1024_600_LCD`
   (1 = painel 1024×600, 0 = 800×480).
5. Configurações da placa (Tools):
   - Board: **ESP32S3 Dev Module**
   - USB CDC On Boot: **Enabled** (obrigatório no modo RTU — ver nota abaixo)
   - Flash Size: **16MB**, Partition Scheme: **16M Flash (3MB APP/9.9MB FATFS)**
   - PSRAM: **OPI PSRAM**
6. Compila e carrega.

### Nota sobre o RS485 (modo RTU)

A porta RS485 da placa usa os **GPIO 43/44**, partilhados com o UART0. O sketch
usa `Serial1` remapeado para esses pinos (como no exemplo `02_RS485_Test`) e o
monitor série passa a funcionar pelo USB nativo — por isso ativa
**USB CDC On Boot**.

## Estrutura

```
DewView/
├── DewView.ino                    # setup/loop: placa, LVGL, WiFi, ciclo de leitura
├── dewview_config.h               # configuração do utilizador
├── s24_modbus.{h,cpp}             # cliente Modbus TCP e RTU (função 0x03)
├── dewview_ui.{h,cpp}             # dashboard LVGL
├── esp_panel_board_custom_conf.h  # configuração da placa (do exemplo 09)
└── lvgl_v8_port.{h,cpp}           # porting LVGL (do exemplo 09)
```
