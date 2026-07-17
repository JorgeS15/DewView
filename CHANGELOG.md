# Changelog do DewView

Todas as alterações relevantes do firmware são registadas aqui. A versão
ativa está definida em `DewView/dewview_config.h` (`DEWVIEW_VERSION`) e é
mostrada no ecrã (cabeçalho e página Sistema), na página web e no arranque
do monitor série.

## v1.1.7 — 2026-07-16

### Novidades

- **Tradução PT/EN**: novo botão no cabeçalho do ecrã que comuta toda a
  interface entre português e inglês. A escolha fica guardada em NVS e
  sobrevive a reinícios.
- **Exportação CSV**: botão "Exportar CSV" na página web (e endpoint
  `/data.csv`) que descarrega as últimas leituras (~5 min a 2 Hz) com
  colunas `uptime_s;temperatura_C;ponto_orvalho_C;humidade_pctHR`
  (separador `;`, decimais `.`).

## v1.1.6 — 2026-07-16

### Correções

- **Painel principal sem valores**: os números grandes eram ampliados com
  `transform_zoom` do LVGL, que não é fiável em labels — os valores ficavam
  invisíveis. Substituído por uma fonte dedicada de **96 px**
  (`dewview_font_96.c`, Montserrat, só dígitos e símbolos), gerada com
  `lv_font_conv`. Os 4 cartões do Painel mostram agora os valores em grande.

### Correções (cont.)

- **Escala vertical dos gráficos cortada**: as etiquetas do eixo Y eram
  desenhadas fora do gráfico e o cartão cortava-as (só se via o último
  dígito). O gráfico foi encolhido e alinhado à direita, reservando 44 px
  para a escala dentro do cartão.

### Alterações

- **Taxa de leitura: 2 Hz** (uma leitura a cada 500 ms, antes 3 s), para uma
  resposta mais imediata no ecrã. O gráfico guarda 600 pontos (~5 min).
  Nota: o S24 amostra internamente a cada 3 s, pelo que valores consecutivos
  podem repetir-se.
- **Páginas web renovadas**: visual escuro igual ao do ecrã, cartões coloridos
  com os valores e o selo de risco, e página de atualização OTA com barra de
  progresso e estado do upload.

## v1.1.5 — 2026-07-15

Correções de comunicação RS-485 (caso "0 bytes recebidos" com o LED laranja
do sensor a piscar).

### Correções

- **GPIO43/44 (partilhados com o UART0)**: os pinos são agora repostos em modo
  GPIO antes de serem entregues ao UART1 do RS-485. Evita que o pad do GPIO43
  fique preso como saída da consola (U0TXD) e bloqueie a receção da resposta
  do sensor.

### Novidades

- **Auto-scan do sensor**: ao fim de 4 falhas consecutivas, o firmware procura
  o S24 em todas as combinações de baud (19200/9600/38400), paridade
  (8N1/8E1/8O1) e endereço Modbus (1–16), adotando automaticamente os
  parâmetros encontrados. O progresso aparece na barra de estado e o resultado
  no registo de Eventos. Repete a cada 30 s enquanto não encontrar.
- A página Sistema mostra os parâmetros de comunicação **ativos** (que podem
  diferir dos configurados, se o auto-scan encontrou o sensor noutro
  baud/endereço).

## v1.1.4 — 2026-07-15

Primeira versão publicada.

### Funcionalidades

- **Dashboard táctil** (Waveshare ESP32-S3-Touch-LCD-5, 1024×600) com 3 páginas:
  - **Painel**: temperatura, ponto de orvalho, humidade e margem T−Td em
    valores grandes, com selo de risco de condensação (Seguro/Alerta/Risco);
  - **Gráficos**: histórico de ~15 min — temperatura + ponto de orvalho num
    gráfico, humidade noutro;
  - **Sistema**: firmware/versão, uptime, memória, rede, contadores Modbus,
    último erro com detalhe e registo de eventos.
- **Sensor Banner S24** (S24AS-D-MQP) por **Modbus RTU** na porta RS-485 da
  placa (predefinido) ou **Modbus TCP** via gateway (cliente Modbus próprio,
  sem bibliotecas externas).
- **Access Point WiFi** `DewView` criado no arranque, com página web de
  estado (http://192.168.4.1).
- **Atualização de firmware OTA**: pela página `/update` (browser, com
  autenticação) ou pela porta de rede do Arduino IDE.
- Diagnóstico de comunicação detalhado: bytes recebidos, dump hexadecimal,
  código de exceção Modbus.

### Notas

- Compilar com core `esp32` 3.x, placa *ESP32S3 Dev Module*, Flash 16MB,
  Partition Scheme *16M Flash (3MB APP/9.9MB FATFS)*, PSRAM OPI.
- O binário `DewView-vX.Y.Z-OTA.bin` das releases é o ficheiro a carregar na
  página `/update`; o `-USB-completo.bin` grava-se por USB (esptool, endereço 0x0).
- Atenção à polaridade RS-485: no S24 **B = +** e **A = −**, na placa
  **A = +** e **B = −** (ver README).
