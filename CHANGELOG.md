# Changelog do DewView

Todas as alterações relevantes do firmware são registadas aqui. A versão
ativa está definida em `DewView/dewview_config.h` (`DEWVIEW_VERSION`) e é
mostrada no ecrã (cabeçalho e página Sistema), na página web e no arranque
do monitor série.

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
