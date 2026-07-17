#include <Preferences.h>
#include "dewview_config.h"
#include "dewview_lang.h"

/* Nota: as fontes LVGL incluidas so cobrem ASCII + simbolo de grau, por isso
 * os textos PT nao usam caracteres acentuados. */

static const DewStrings PT = {
    /* Cartoes */
    "Temperatura", "Ponto de Orvalho", "Humidade", "Margem T - Td",
    /* Separadores */
    "Painel", "Graficos", "Sistema",
    /* Graficos */
    "Temperatura (°C)", "Ponto de orvalho (°C)", "Humidade (% HR)",
    /* Selo */
    "Seguro", "Alerta", "Risco", "Sem dados",
    /* Sistema */
    "Sistema", "Rede", "Modbus", "Eventos", "(sem eventos)",
    "Firmware: DewView v" DEWVIEW_VERSION " (%s %s)\n"
    "Desenvolvedor: " DEWVIEW_DEVELOPER "\n"
    "Uptime: %s\n"
    "Heap livre: %u KB (min %u KB)\n"
    "PSRAM livre: %u KB\n"
    "Ecra: %dx%d",
    "AP: %s (%s)\nClientes no AP: %d\nOTA: /update ou porta de rede",
    "AP: %s (%s)\nClientes no AP: %d\nSTA: %s (%s)\nOTA: /update ou porta de rede",
    "Modo: %s\nLeituras OK: %lu\nFalhas: %lu\nUltimo erro: %s",
    /* Cabecalho / estados */
    "Sensor S24  -  Ponto de Orvalho  |  v" DEWVIEW_VERSION,
    "A iniciar...",
    "A ligar WiFi...",
    "Sensor OK  |  AP %s",
    "Sensor OK  |  %s",
    "A procurar sensor: %lu %s...",
    "Sensor nao encontrado",
    /* Eventos */
    "Arranque",
    "Sensor recuperado",
    "Sensor sem resposta (%s)",
    "Sensor encontrado: %lu %s addr %u",
    "A procurar sensor no barramento...",
    "Procura sem resposta: verificar cablagem",
};

static const DewStrings EN = {
    /* Tiles */
    "Temperature", "Dew Point", "Humidity", "Margin T - Td",
    /* Tabs */
    "Dashboard", "Charts", "System",
    /* Charts */
    "Temperature (°C)", "Dew point (°C)", "Humidity (% RH)",
    /* Badge */
    "Safe", "Warning", "Risk", "No data",
    /* System */
    "System", "Network", "Modbus", "Events", "(no events)",
    "Firmware: DewView v" DEWVIEW_VERSION " (%s %s)\n"
    "Developer: " DEWVIEW_DEVELOPER "\n"
    "Uptime: %s\n"
    "Free heap: %u KB (min %u KB)\n"
    "Free PSRAM: %u KB\n"
    "Screen: %dx%d",
    "AP: %s (%s)\nAP clients: %d\nOTA: /update or IDE network port",
    "AP: %s (%s)\nAP clients: %d\nSTA: %s (%s)\nOTA: /update or IDE network port",
    "Mode: %s\nGood reads: %lu\nFailures: %lu\nLast error: %s",
    /* Header / states */
    "S24 Sensor  -  Dew Point  |  v" DEWVIEW_VERSION,
    "Starting...",
    "Connecting to WiFi...",
    "Sensor OK  |  AP %s",
    "Sensor OK  |  %s",
    "Scanning for sensor: %lu %s...",
    "Sensor not found",
    /* Events */
    "Boot",
    "Sensor recovered",
    "Sensor not responding (%s)",
    "Sensor found: %lu %s addr %u",
    "Scanning bus for sensor...",
    "Scan finished, no response: check wiring",
};

static bool s_is_pt = true;

const DewStrings *dew_tr()
{
    return s_is_pt ? &PT : &EN;
}

bool dew_lang_is_pt()
{
    return s_is_pt;
}

void dew_lang_toggle()
{
    s_is_pt = !s_is_pt;
    Preferences prefs;
    if (prefs.begin("dewview", false)) {
        prefs.putUChar("lang", s_is_pt ? 0 : 1);
        prefs.end();
    }
}

void dew_lang_load()
{
    Preferences prefs;
    if (prefs.begin("dewview", true)) {
        s_is_pt = (prefs.getUChar("lang", 0) == 0);
        prefs.end();
    }
}
