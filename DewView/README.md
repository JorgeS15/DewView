# DewView — Dashboard de Ponto de Orvalho

Dashboard para o módulo **Waveshare ESP32-S3-Touch-LCD-5** (1024×600 ou 800×480)
que mostra em tempo real a **temperatura**, o **ponto de orvalho** e a **humidade**
lidos de um sensor **Banner S24 Dew Point (S24AS-D-MQP)** por Modbus.

A interface é **touch**, com 3 páginas (separadores no fundo do ecrã, também
navegáveis por deslize):

1. **Painel** — 4 cartões com valores grandes, legíveis à distância:
   Temperatura, Ponto de Orvalho, Humidade e **Margem T−Td** (risco de
   condensação, com selo Seguro / Alerta / Risco);
2. **Gráficos** — histórico de ~15 min (leituras a cada 3 s): temperatura +
   ponto de orvalho num gráfico e humidade noutro;
3. **Sistema** — diagnóstico: firmware, uptime, memória, rede/AP, estado do
   Modbus (contadores de leituras/falhas, último erro) e registo de eventos.

O cabeçalho mostra sempre o estado da ligação ao sensor.

## Rede e atualização de firmware (OTA)

Ao arrancar, a placa cria sempre a rede WiFi **`DewView`** (password **`Orvalho88`**,
configurável em `dewview_config.h`). Ligado a essa rede:

- **http://192.168.4.1** (ou `http://dewview.local`) — página de estado com as
  leituras atuais;
- **http://192.168.4.1/update** — atualização de firmware pelo browser: pede
  utilizador/password (`admin` / `Orvalho88`), escolhe o `.bin` exportado do
  Arduino IDE (*Sketch → Export Compiled Binary*, usar o ficheiro
  `*.ino.bin`) e submete. A placa reinicia sozinha no fim;
- **Arduino IDE** — a placa também aparece como *network port* (ArduinoOTA,
  password `Orvalho88`), para carregar diretamente do IDE sem cabo.

No modo TCP a placa funciona em AP+STA: mantém o AP `DewView` e liga-se em
simultâneo à rede do gateway Modbus.

> **Nota:** o esquema de partições tem de ter duas partições de aplicação
> (OTA). O recomendado abaixo — *16M Flash (3MB APP/9.9MB FATFS)* — serve.

## Modos de ligação ao sensor

O S24 é um sensor **Modbus RTU sobre RS-485** nativo. Há duas formas de o ligar:

| Modo | Como | Configuração |
|------|------|--------------|
| **RTU (predefinido)** | Sensor ligado diretamente ao terminal RS485 (A/B) da placa. Não precisa de rede. | `DEWVIEW_MODBUS_MODE DEWVIEW_MODBUS_RTU` |
| **TCP** | Sensor ligado a um gateway RS485→Modbus TCP (ex.: USR-DR302, Moxa MGate); a placa liga-se por WiFi. | `DEWVIEW_MODBUS_MODE DEWVIEW_MODBUS_TCP` |

### Ligações do S24 (modo RTU)

⚠️ **Atenção à polaridade**: a Banner e a Waveshare usam a letra A/B ao
contrário uma da outra! No S24, **B = +** e **A = −**; no transceiver da placa
(SP3485), **A = +** e **B = −**. Liga pelos sinais **+/−**, não pelas letras:

| Pino S24 | Cor | Sinal | Terminal da placa |
|----------|-----|-------|-------------------|
| 1 | Castanho | +10…30 V DC | — (fonte externa, ex.: 24 V) |
| 2 | Branco | RS485 **+** (D1/B do sensor) | **A** |
| 3 | Azul | GND | GND (comum com a fonte e com a placa) |
| 4 | Preto | RS485 **−** (D0/A do sensor) | **B** |

Se não comunicar, o primeiro teste é **trocar os fios branco e preto** — a
troca de polaridade não danifica nada, simplesmente não comunica.

**LEDs do sensor**: verde aceso = alimentado; laranja a piscar em sincronia
com as leituras = os pedidos estão a chegar ao sensor (problema no retorno ou
nos parâmetros). Desde a v1.1.5, se o sensor não responder o firmware faz um
**auto-scan** de baud/paridade/endereço e adota os parâmetros que encontrar
(resultado no registo de Eventos da página Sistema).

Parâmetros de fábrica do S24: endereço **1**, **19200** baud, sem paridade (8N1).

**Terminação de 120 Ω**: a placa já tem as resistências de terminação (R61/R15,
120 Ω) selecionáveis pelo pequeno **interruptor DIP (SW2)** junto aos terminais
RS485/CAN — não é preciso soldar nada. Para um cabo curto a 19200 baud a
terminação é dispensável; para cabos longos (>10 m), liga a posição do RS485.
As resistências de polarização (bias, 4,7 kΩ) já estão na placa.

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

### Erro "Sketch too big" / "text section exceeds available space"

Este erro significa que o **Partition Scheme** em Tools ficou no predefinido
(1,3 MB para a aplicação). O módulo tem um ESP32-S3-WROOM-1 **N16R8** com 16 MB
de flash: seleciona **Flash Size: 16MB (128Mb)** e **Partition Scheme:
16M Flash (3MB APP/9.9MB FATFS)**. Além de resolver o erro, este esquema tem as
duas partições de aplicação necessárias para o OTA.

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
├── dewview_net.{h,cpp}            # AP WiFi, página web de estado e OTA
├── dewview_ui.{h,cpp}             # dashboard LVGL
├── esp_panel_board_custom_conf.h  # configuração da placa (do exemplo 09)
└── lvgl_v8_port.{h,cpp}           # porting LVGL (do exemplo 09)
```
