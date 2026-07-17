#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>

#include "dewview_config.h"
#include "dewview_net.h"

static WebServer s_server(80);
static S24Reading s_last_reading = {};
static bool s_last_ok = false;
static volatile bool s_ota_busy = false;

/* Historico circular das ultimas leituras, para exportacao em CSV */
struct Sample {
    uint32_t ms;      // uptime da leitura
    float tempC, dewC, humidity;
};
static constexpr uint16_t HIST_MAX = 600;   // = pontos dos graficos (~5 min a 2 Hz)
static Sample s_hist[HIST_MAX];
static uint16_t s_hist_len = 0;
static uint16_t s_hist_next = 0;

/*------------------------------ Paginas web ------------------------------*/

/* Cabeca comum (CSS com a mesma paleta do ecra). Fora do snprintf para nao
 * ter de escapar os '%' do CSS. */
static const char PAGE_HEAD[] PROGMEM = R"raw(<!DOCTYPE html><html lang="pt"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DewView</title><style>
:root{--bg:#111110;--card:#1a1a19;--tx:#fff;--tx2:#c3c2b7;--mut:#8a897e;
--temp:#d95926;--dew:#3987e5;--rh:#199e70;--ok:#2ea36e;--warn:#c98500;--crit:#e66767}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--tx);font-family:system-ui,-apple-system,sans-serif;
min-height:100vh;display:flex;flex-direction:column}
header{background:var(--card);padding:14px 20px;display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}
header h1{font-size:22px;letter-spacing:.5px}
header small{color:var(--mut)}
main{flex:1;width:100%;max-width:860px;margin:0 auto;padding:16px;display:grid;
grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px;align-content:start}
.card{background:var(--card);border-radius:12px;padding:18px;border-left:6px solid var(--mut)}
.card h2{font-size:14px;font-weight:600;color:var(--tx2);text-transform:uppercase;letter-spacing:.05em}
.card .v{font-size:52px;font-weight:700;margin-top:6px;font-variant-numeric:tabular-nums}
.card .v small{font-size:18px;color:var(--mut);font-weight:400}
.t{border-color:var(--temp)}.d{border-color:var(--dew)}.h{border-color:var(--rh)}
.badge{display:inline-block;margin-top:10px;padding:5px 14px;border-radius:16px;
color:#111;font-size:13px;font-weight:700}
.hint{color:var(--tx2);font-size:14px;line-height:1.5;margin:10px 0 16px}
.btn{background:var(--dew);color:#fff;border:0;border-radius:8px;padding:12px 22px;
font-size:15px;font-weight:600;cursor:pointer;width:100%}
.btn:disabled{background:var(--mut)}
input[type=file]{color:var(--tx2);width:100%;padding:10px;background:var(--bg);
border:1px dashed var(--mut);border-radius:8px}
progress{width:100%;height:12px;margin-top:14px;accent-color:var(--dew)}
#s{color:var(--tx2);margin-top:10px;font-size:14px}
footer{color:var(--mut);text-align:center;padding:14px;font-size:12px}
footer a{color:var(--dew);text-decoration:none}
</style></head><body>
<header><h1>DewView</h1><small>Sensor S24 &middot; Ponto de Orvalho</small></header>
)raw";

static const char PAGE_FOOT[] PROGMEM =
    "<footer><a href=\"/update\">Atualizar firmware</a> &middot; "
    "DewView v" DEWVIEW_VERSION " &mdash; " DEWVIEW_DEVELOPER "</footer></body></html>";

static void send_page(const char *body)
{
    s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server.send(200, "text/html", "");
    s_server.sendContent_P(PAGE_HEAD);
    s_server.sendContent(body);
    s_server.sendContent_P(PAGE_FOOT);
    s_server.sendContent("");  // termina a resposta chunked
}

static void format_value(char *buf, size_t len, float v)
{
    snprintf(buf, len, "%.1f", v);
}

static void handle_root()
{
    char t[16] = "--", d[16] = "--", h[16] = "--", m[16] = "--";
    const char *badge_color = "var(--warn)";
    const char *badge_text = "Sem dados";

    if (s_last_ok) {
        const float margin = s_last_reading.tempC - s_last_reading.dewC;
        format_value(t, sizeof(t), s_last_reading.tempC);
        format_value(d, sizeof(d), s_last_reading.dewC);
        format_value(h, sizeof(h), s_last_reading.humidity);
        format_value(m, sizeof(m), margin);
        if (margin >= DEWVIEW_MARGIN_OK) {
            badge_color = "var(--ok)";
            badge_text = "Seguro";
        } else if (margin >= DEWVIEW_MARGIN_WARN) {
            badge_color = "var(--warn)";
            badge_text = "Alerta";
        } else {
            badge_color = "var(--crit)";
            badge_text = "Risco de condensacao";
        }
    }

    char body[1024];
    snprintf(body, sizeof(body),
             "<main>"
             "<div class=\"card t\"><h2>Temperatura</h2><p class=\"v\">%s <small>&deg;C</small></p></div>"
             "<div class=\"card d\"><h2>Ponto de Orvalho</h2><p class=\"v\">%s <small>&deg;C</small></p></div>"
             "<div class=\"card h\"><h2>Humidade</h2><p class=\"v\">%s <small>%%HR</small></p></div>"
             "<div class=\"card\"><h2>Margem T&minus;Td</h2><p class=\"v\">%s <small>&deg;C</small></p>"
             "<span class=\"badge\" style=\"background:%s\">%s</span></div>"
             "<div class=\"card\" style=\"display:flex;align-items:center\">"
             "<a class=\"btn\" style=\"text-align:center;text-decoration:none\" "
             "href=\"/data.csv\" download>Exportar CSV (ultimos 5 min)</a></div>"
             "</main>"
             /* auto-refresh so na pagina de estado (JS para nao afetar /update) */
             "<script>setTimeout(()=>location.reload(),5000)</script>",
             t, d, h, m, badge_color, badge_text);

    send_page(body);
}

static bool check_auth()
{
    if (!s_server.authenticate(DEWVIEW_OTA_USER, DEWVIEW_OTA_PASSWORD)) {
        s_server.requestAuthentication();
        return false;
    }
    return true;
}

static void handle_update_form()
{
    if (!check_auth()) {
        return;
    }
    static const char body[] =
        "<main style=\"grid-template-columns:1fr;max-width:560px\">"
        "<div class=\"card d\"><h2>Atualizar firmware</h2>"
        "<p class=\"hint\">Escolhe o ficheiro <b>DewView-vX.Y.Z-OTA.bin</b> "
        "(das releases do GitHub ou exportado do Arduino IDE) e carrega em Atualizar. "
        "A placa reinicia sozinha no fim.</p>"
        "<form id=\"f\"><input type=\"file\" name=\"firmware\" accept=\".bin\" required>"
        "<br><br><button class=\"btn\" type=\"submit\">Atualizar</button></form>"
        "<progress id=\"p\" max=\"100\" value=\"0\" hidden></progress><p id=\"s\"></p>"
        "</div></main>"
        "<script>"
        "const f=document.getElementById('f'),p=document.getElementById('p'),"
        "s=document.getElementById('s'),b=f.querySelector('button');"
        "f.addEventListener('submit',e=>{e.preventDefault();"
        "const x=new XMLHttpRequest();const d=new FormData(f);"
        "p.hidden=false;b.disabled=true;s.textContent='A enviar...';"
        "x.open('POST','/update');"
        "x.upload.onprogress=ev=>{if(ev.lengthComputable)p.value=ev.loaded/ev.total*100};"
        "x.onload=()=>{if(x.status==200){s.textContent='Atualizado! A placa esta a reiniciar; "
        "aguarda ~20 s e volta a ligar-te a rede DewView.';}"
        "else{s.textContent='Falha na atualizacao ('+x.status+'). Ve a pagina Sistema.';b.disabled=false;}};"
        "x.onerror=()=>{s.textContent='Ligacao perdida (normal se a placa ja reiniciou).';};"
        "x.send(d);});"
        "</script>";
    send_page(body);
}

static void handle_update_done()
{
    if (!check_auth()) {
        return;
    }
    s_ota_busy = false;
    if (Update.hasError()) {
        s_server.send(500, "text/plain", "Falha na atualizacao.");
    } else {
        s_server.client().setNoDelay(true);
        s_server.send(200, "text/plain", "OK");
        delay(500);
        ESP.restart();
    }
}

static void handle_update_upload()
{
    HTTPUpload &upload = s_server.upload();

    switch (upload.status) {
    case UPLOAD_FILE_START:
        s_ota_busy = true;
        Serial.printf("OTA web: inicio (%s)\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
        break;
    case UPLOAD_FILE_WRITE:
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        break;
    case UPLOAD_FILE_END:
        if (Update.end(true)) {
            Serial.printf("OTA web: concluido, %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
        break;
    case UPLOAD_FILE_ABORTED:
        Update.abort();
        s_ota_busy = false;
        Serial.println("OTA web: abortado");
        break;
    default:
        break;
    }
}

/* Exporta o historico como CSV (separador ';', decimais '.').
 * uptime_s e o instante da leitura em segundos desde o arranque. */
static void handle_data_csv()
{
    s_server.sendHeader("Content-Disposition", "attachment; filename=dewview.csv");
    s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server.send(200, "text/csv", "");
    s_server.sendContent("uptime_s;temperatura_C;ponto_orvalho_C;humidade_pctHR\r\n");

    char chunk[1024];
    size_t used = 0;
    const uint16_t start = (s_hist_next + HIST_MAX - s_hist_len) % HIST_MAX;
    for (uint16_t i = 0; i < s_hist_len; i++) {
        const Sample &sm = s_hist[(start + i) % HIST_MAX];
        used += snprintf(chunk + used, sizeof(chunk) - used, "%lu.%03lu;%.1f;%.1f;%.1f\r\n",
                         (unsigned long)(sm.ms / 1000), (unsigned long)(sm.ms % 1000),
                         sm.tempC, sm.dewC, sm.humidity);
        if (used > sizeof(chunk) - 48) {
            s_server.sendContent(chunk, used);
            used = 0;
        }
    }
    if (used > 0) {
        s_server.sendContent(chunk, used);
    }
    s_server.sendContent("");
}

/*--------------------------------- API -----------------------------------*/

void dewview_net_begin()
{
#if DEWVIEW_MODBUS_MODE == DEWVIEW_MODBUS_TCP
    /* AP proprio + ligacao a rede do gateway Modbus */
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(DEWVIEW_WIFI_SSID, DEWVIEW_WIFI_PASSWORD);
    WiFi.setAutoReconnect(true);
#else
    WiFi.mode(WIFI_AP);
#endif
    WiFi.softAP(DEWVIEW_AP_SSID, DEWVIEW_AP_PASSWORD);
    Serial.printf("AP \"%s\" ativo, IP %s\n",
                  DEWVIEW_AP_SSID, WiFi.softAPIP().toString().c_str());

    if (MDNS.begin(DEWVIEW_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
    }

    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/data.csv", HTTP_GET, handle_data_csv);
    s_server.on("/update", HTTP_GET, handle_update_form);
    s_server.on("/update", HTTP_POST, handle_update_done, handle_update_upload);
    s_server.onNotFound([]() {
        s_server.send(404, "text/plain", "404");
    });
    s_server.begin();

    /* OTA por rede a partir do Arduino IDE (porta de rede) */
    ArduinoOTA.setHostname(DEWVIEW_HOSTNAME);
    ArduinoOTA.setPassword(DEWVIEW_OTA_PASSWORD);
    ArduinoOTA
    .onStart([]() {
        s_ota_busy = true;
        Serial.println("OTA IDE: inicio");
    })
    .onEnd([]() {
        Serial.println("OTA IDE: concluido");
    })
    .onError([](ota_error_t error) {
        s_ota_busy = false;
        Serial.printf("OTA IDE: erro %u\n", error);
    });
    ArduinoOTA.begin();
}

void dewview_net_loop()
{
    s_server.handleClient();
    ArduinoOTA.handle();
}

void dewview_net_set_reading(const S24Reading &reading, bool ok)
{
    s_last_reading = reading;
    s_last_ok = ok;

    if (ok) {
        s_hist[s_hist_next] = {millis(), reading.tempC, reading.dewC, reading.humidity};
        s_hist_next = (s_hist_next + 1) % HIST_MAX;
        if (s_hist_len < HIST_MAX) {
            s_hist_len++;
        }
    }
}

bool dewview_net_ota_busy()
{
    return s_ota_busy;
}
