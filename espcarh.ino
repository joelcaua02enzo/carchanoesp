/*
  Crachá GitHub OTIMIZADO para ESP32-S3
  - Reduz consumo de RAM
  - Evita travamentos em requisições HTTPS
  - Usa TOKEN do GitHub (aumenta limite de 60 para 5000 req/hora)
  - Limita número de repositórios consultados
  - Modo RETRATO (240x320)
  - QR Code dentro do cartão
  - Scroll vertical
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Preferences.h>
#include <lvgl.h>

// ========== CONFIGURAÇÕES ==========
// *** COLE AQUI O SEU TOKEN DO GITHUB (opcional, mas recomendado) ***
// Deixe vazio ("") se não quiser usar.
// Como criar: https://github.com/settings/tokens
// Escopo necessário: "public_repo" ou "read:user"
const char* githubToken = "";   // <-- COLE O TOKEN AQUI (ex: "ghp_xxxxxxxxxxxx")

// ========== PINOS DO TOUCH ==========
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36

// ========== DIMENSÕES ==========
#define LARGURA 240
#define ALTURA  320

// ========== LIMITES ==========
#define MAX_LINGUAGENS       8
#define MAX_REPOS_CONSULTAR  5     // Reduzido para 5 (menos requisições)
#define MAX_HISTORICO        5

// ========== OBJETOS ==========
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
WiFiClientSecure client;
Preferences prefs;

// ========== BUFFERS LVGL ==========
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LARGURA * 6];

// ========== CONFIGURAÇÕES SALVAS ==========
String cfg_ssid = "";
String cfg_password = "";
String cfg_github = "";

// ========== DADOS DO GITHUB ==========
struct GitHubData {
  String login;
  String name;
  String bio;
  String location;
  int followers;
  int publicRepos;
  bool valido;
};
GitHubData perfil;

// ========== LINGUAGENS ==========
struct Linguagem {
  String nome;
  long bytes;
};
Linguagem linguagens[MAX_LINGUAGENS];
int numLinguagens = 0;

// ========== ESTADO ==========
enum TelaAtual {
  TELA_LISTA_WIFI,
  TELA_CONFIG_SENHA,
  TELA_CONFIG_GITHUB,
  TELA_PRINCIPAL,
  TELA_CARREGANDO
};
TelaAtual telaAtual = TELA_LISTA_WIFI;

String ssidSelecionado = "";
unsigned long lastRefresh = 0;
const unsigned long REFRESH_INTERVAL = 300000;   // *** 5 MINUTOS ***

// ========== OBJETOS LVGL ==========
lv_obj_t * textarea_input;
lv_obj_t * keyboard;

// ========== CORES ==========
#define COR_FUNDO_1    lv_color_hex(0x0841)
#define COR_FUNDO_2    lv_color_hex(0x10A3)
#define COR_CARD       lv_color_hex(0x18E3)
#define COR_CARD_BG    lv_color_hex(0x2124)
#define COR_BORDA      lv_color_hex(0x3186)
#define COR_DESTAQUE   lv_color_hex(0x07FF)
#define COR_AVATAR     lv_color_hex(0x6C1F)
#define COR_VERDE      lv_color_hex(0x07E0)
#define COR_AMARELO    lv_color_hex(0xFFE0)
#define COR_VERMELHO   lv_color_hex(0xF800)
#define COR_BRANCO     lv_color_hex(0xFFFF)
#define COR_CINZA      lv_color_hex(0x8410)

// ========== PROTÓTIPOS ==========
void criarTelaListaWiFi();
void criarTelaPrincipal();
void criarTelaLoading(String msg);
void criarTelaSenha();
void criarTelaGitHub();
static void keyboard_event_cb(lv_event_t * e);
static void btn_config_event(lv_event_t * e);
static void rede_selecionada_event(lv_event_t * e);

// ========== LOG ==========
void logMemoria(const char* onde) {
  Serial.printf("[%s] Heap livre: %d bytes\n", onde, ESP.getFreeHeap());
}

// ========== FLUSH DO DISPLAY ==========
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ========== LEITURA DO TOUCH ==========
void my_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    data->point.x = map(p.x, 200, 3700, 0, LARGURA);
    data->point.y = map(p.y, 200, 3700, 0, ALTURA);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ========== CARREGA CONFIG ==========
void carregarConfig() {
  prefs.begin("cracha", true);
  cfg_ssid     = prefs.getString("ssid", "");
  cfg_password = prefs.getString("password", "");
  cfg_github   = prefs.getString("github", "");
  prefs.end();
}

// ========== SALVA CONFIG ==========
void salvarConfig() {
  prefs.begin("cracha", false);
  prefs.putString("ssid", cfg_ssid);
  prefs.putString("password", cfg_password);
  prefs.putString("github", cfg_github);
  prefs.end();
  Serial.println("Config salva!");
}

// ========== DIAGNÓSTICO DE ERRO HTTP ==========
void diagnosticoHTTP(HTTPClient &https, int httpCode) {
  Serial.printf("Erro HTTP: %d\n", httpCode);
  String remaining = https.header("x-ratelimit-remaining");
  String reset = https.header("x-ratelimit-reset");
  String sso = https.header("X-GitHub-SSO");

  if (remaining.length() > 0) {
    Serial.printf("  Rate limit restante: %s\n", remaining.c_str());
  }
  if (reset.length() > 0) {
    Serial.printf("  Reset em (epoch): %s\n", reset.c_str());
  }
  if (sso.length() > 0) {
    Serial.printf("  SSO: %s\n", sso.c_str());
  }

  if (httpCode == 403) {
    Serial.println("  >> 403: limite excedido ou token invalido.");
    Serial.println("  >> Solucao: use um token do GitHub ou aumente o intervalo.");
  } else if (httpCode == 404) {
    Serial.println("  >> 404: usuario nao encontrado.");
  } else if (httpCode == 401) {
    Serial.println("  >> 401: token invalido.");
  }
}

// ========== BUSCA GITHUB ==========
bool buscarGitHub() {
  if (WiFi.status() != WL_CONNECTED || cfg_github.length() == 0) return false;

  logMemoria("antes buscarGitHub");

  HTTPClient https;
  String url = "https://api.github.com/users/" + cfg_github;
  if (!https.begin(client, url)) {
    Serial.println("Falha no https.begin()");
    return false;
  }

  https.setTimeout(10000);
  https.addHeader("User-Agent", "ESP32-Cracha");
  https.addHeader("Accept", "application/vnd.github.v3+json");
  https.useHTTP10(true);

  // *** ADICIONA O TOKEN SE TIVER ***
  if (strlen(githubToken) > 0) {
    String auth = "Bearer ";
    auth += githubToken;
    https.addHeader("Authorization", auth);
    Serial.println("Usando token do GitHub");
  }

  int httpCode = https.GET();
  if (httpCode != 200) {
    diagnosticoHTTP(https, httpCode);
    https.end();
    client.stop();
    return false;
  }

  String payload = https.getString();
  https.end();
  client.stop();

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("Erro JSON: %s\n", err.c_str());
    return false;
  }

  perfil.login       = doc["login"].as<String>();
  perfil.name        = doc["name"].as<String>();
  perfil.bio         = doc["bio"].as<String>();
  perfil.location    = doc["location"].as<String>();
  perfil.followers   = doc["followers"].as<int>();
  perfil.publicRepos = doc["public_repos"].as<int>();
  perfil.valido      = true;

  payload = "";
  doc.clear();

  logMemoria("depois buscarGitHub");
  return true;
}

// ========== BUSCA LINGUAGENS ==========
bool buscarLinguagens() {
  if (WiFi.status() != WL_CONNECTED || cfg_github.length() == 0) return false;

  numLinguagens = 0;
  logMemoria("antes buscarLinguagens");

  HTTPClient https;
  String url = "https://api.github.com/users/" + cfg_github + "/repos?per_page=30&sort=updated";
  if (!https.begin(client, url)) return false;

  https.setTimeout(10000);
  https.addHeader("User-Agent", "ESP32-Cracha");
  https.addHeader("Accept", "application/vnd.github.v3+json");
  https.useHTTP10(true);

  if (strlen(githubToken) > 0) {
    String auth = "Bearer ";
    auth += githubToken;
    https.addHeader("Authorization", auth);
  }

  int httpCode = https.GET();
  if (httpCode != 200) {
    diagnosticoHTTP(https, httpCode);
    https.end();
    client.stop();
    return false;
  }

  String payload = https.getString();
  https.end();
  client.stop();

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("Erro JSON repos: %s\n", err.c_str());
    return false;
  }
  payload = "";

  String urls[MAX_REPOS_CONSULTAR];
  int numUrls = 0;

  for (JsonObject repo : doc.as<JsonArray>()) {
    if (numUrls >= MAX_REPOS_CONSULTAR) break;
    if (repo["fork"].as<bool>()) continue;

    String langUrl = repo["languages_url"].as<String>();
    if (langUrl.length() > 0) {
      urls[numUrls++] = langUrl;
    }
  }
  doc.clear();
  logMemoria("depois lista repos");

  String nomesTemp[MAX_LINGUAGENS];
  long bytesTemp[MAX_LINGUAGENS];
  int numTemp = 0;

  for (int r = 0; r < numUrls; r++) {
    HTTPClient https2;
    if (!https2.begin(client, urls[r])) continue;
    https2.setTimeout(8000);
    https2.addHeader("User-Agent", "ESP32-Cracha");
    https2.addHeader("Accept", "application/vnd.github.v3+json");
    https2.useHTTP10(true);

    if (strlen(githubToken) > 0) {
      String auth = "Bearer ";
      auth += githubToken;
      https2.addHeader("Authorization", auth);
    }

    int code = https2.GET();
    if (code != 200) {
      https2.end();
      client.stop();
      delay(50);
      continue;
    }

    String payload2 = https2.getString();
    https2.end();
    client.stop();

    DynamicJsonDocument doc2(1024);
    if (deserializeJson(doc2, payload2)) {
      delay(50);
      continue;
    }

    for (JsonPair kv : doc2.as<JsonObject>()) {
      String nomeLang = kv.key().c_str();
      long bytes = kv.value().as<long>();

      bool achou = false;
      for (int i = 0; i < numTemp; i++) {
        if (nomesTemp[i] == nomeLang) {
          bytesTemp[i] += bytes;
          achou = true;
          break;
        }
      }
      if (!achou && numTemp < MAX_LINGUAGENS) {
        nomesTemp[numTemp] = nomeLang;
        bytesTemp[numTemp] = bytes;
        numTemp++;
      }
    }
    delay(100);
  }

  // Ordena
  for (int i = 0; i < numTemp - 1; i++) {
    for (int j = i + 1; j < numTemp; j++) {
      if (bytesTemp[j] > bytesTemp[i]) {
        long tb = bytesTemp[i]; bytesTemp[i] = bytesTemp[j]; bytesTemp[j] = tb;
        String tn = nomesTemp[i]; nomesTemp[i] = nomesTemp[j]; nomesTemp[j] = tn;
      }
    }
  }

  numLinguagens = min(numTemp, MAX_HISTORICO);
  for (int i = 0; i < numLinguagens; i++) {
    linguagens[i].nome = nomesTemp[i];
    linguagens[i].bytes = bytesTemp[i];
  }

  Serial.printf("Linguagens: %d\n", numLinguagens);
  logMemoria("depois buscarLinguagens");
  return true;
}

// ========== CONECTA WIFI ==========
bool conectarWiFi() {
  if (cfg_ssid.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg_ssid.c_str(), cfg_password.c_str());

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
    return true;
  }
  return false;
}

// ========== TELA DE CARREGANDO ==========
void criarTelaLoading(String msg) {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), COR_FUNDO_1, 0);

  lv_obj_t * label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, msg.c_str());
  lv_obj_set_style_text_color(label, COR_BRANCO, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);

  lv_obj_t * spinner = lv_spinner_create(lv_scr_act(), 1000, 60);
  lv_obj_set_size(spinner, 40, 40);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 60);
  lv_obj_set_style_arc_color(spinner, COR_DESTAQUE, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(spinner, COR_BORDA, LV_PART_MAIN);

  for (int i = 0; i < 30; i++) {
    lv_timer_handler();
    delay(10);
  }
}

// ========== TELA DE SENHA ==========
void criarTelaSenha() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), COR_FUNDO_1, 0);

  lv_obj_t * header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, LARGURA, 50);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(header, COR_FUNDO_2, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  lv_obj_t * titulo = lv_label_create(header);
  lv_label_set_text(titulo, LV_SYMBOL_WIFI "  SENHA WIFI");
  lv_obj_set_style_text_color(titulo, COR_BRANCO, 0);
  lv_obj_set_style_text_font(titulo, &lv_font_montserrat_20, 0);
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t * lbl_ssid = lv_label_create(lv_scr_act());
  lv_label_set_text(lbl_ssid, ssidSelecionado.c_str());
  lv_obj_set_style_text_color(lbl_ssid, COR_DESTAQUE, 0);
  lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 10, 60);

  textarea_input = lv_textarea_create(lv_scr_act());
  lv_obj_set_size(textarea_input, 220, 35);
  lv_obj_align(textarea_input, LV_ALIGN_TOP_MID, 0, 90);
  lv_textarea_set_one_line(textarea_input, true);
  lv_textarea_set_password_mode(textarea_input, true);
  lv_obj_set_style_text_font(textarea_input, &lv_font_montserrat_14, 0);
  lv_obj_set_style_border_color(textarea_input, COR_DESTAQUE, 0);
  lv_obj_set_style_border_width(textarea_input, 2, 0);
  lv_obj_set_style_radius(textarea_input, 8, 0);

  keyboard = lv_keyboard_create(lv_scr_act());
  lv_obj_set_size(keyboard, 230, 160);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboard, textarea_input);
  lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_14, 0);

  lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
}

// ========== TELA DO GITHUB ==========
void criarTelaGitHub() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), COR_FUNDO_1, 0);

  lv_obj_t * header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, LARGURA, 50);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(header, COR_FUNDO_2, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  lv_obj_t * titulo = lv_label_create(header);
  lv_label_set_text(titulo, LV_SYMBOL_HOME "  GITHUB");
  lv_obj_set_style_text_color(titulo, COR_BRANCO, 0);
  lv_obj_set_style_text_font(titulo, &lv_font_montserrat_20, 0);
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, 10, 0);

  textarea_input = lv_textarea_create(lv_scr_act());
  lv_obj_set_size(textarea_input, 220, 35);
  lv_obj_align(textarea_input, LV_ALIGN_TOP_MID, 0, 90);
  lv_textarea_set_one_line(textarea_input, true);
  lv_textarea_set_text(textarea_input, cfg_github.c_str());
  lv_obj_set_style_text_font(textarea_input, &lv_font_montserrat_14, 0);
  lv_obj_set_style_border_color(textarea_input, COR_DESTAQUE, 0);
  lv_obj_set_style_border_width(textarea_input, 2, 0);
  lv_obj_set_style_radius(textarea_input, 8, 0);

  keyboard = lv_keyboard_create(lv_scr_act());
  lv_obj_set_size(keyboard, 230, 160);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboard, textarea_input);
  lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_14, 0);

  lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
}

// ========== CALLBACK DO TECLADO ==========
static void keyboard_event_cb(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_READY) return;

  String valor = String(lv_textarea_get_text(textarea_input));

  if (telaAtual == TELA_CONFIG_SENHA) {
    cfg_password = valor;
    cfg_ssid = ssidSelecionado;
    Serial.println("Senha salva. SSID: " + cfg_ssid);

    telaAtual = TELA_CONFIG_GITHUB;
    criarTelaGitHub();
  }
  else if (telaAtual == TELA_CONFIG_GITHUB) {
    cfg_github = valor;
    salvarConfig();

    criarTelaLoading("Conectando WiFi...");
    if (conectarWiFi()) {
      criarTelaLoading("Buscando GitHub...");
      if (buscarGitHub()) {
        criarTelaLoading("Buscando linguagens...");
        buscarLinguagens();

        telaAtual = TELA_PRINCIPAL;
        criarTelaPrincipal();
        lastRefresh = millis();
      } else {
        criarTelaLoading("Erro ao buscar GitHub");
      }
    } else {
      criarTelaLoading("Erro ao conectar WiFi");
    }
  }
}

// ========== CALLBACK: REDE SELECIONADA ==========
static void rede_selecionada_event(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  lv_obj_t * btn = lv_event_get_target(e);
  const char * ssid = (const char *) lv_obj_get_user_data(btn);

  ssidSelecionado = String(ssid);

  if (ssid) free((void*)ssid);

  Serial.println("Rede selecionada: " + ssidSelecionado);

  telaAtual = TELA_CONFIG_SENHA;
  criarTelaSenha();
}

// ========== TELA DE LISTA DE REDES ==========
void criarTelaListaWiFi() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), COR_FUNDO_1, 0);

  logMemoria("antes scan WiFi");

  lv_obj_t * header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, LARGURA, 50);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(header, COR_FUNDO_2, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  lv_obj_t * titulo = lv_label_create(header);
  lv_label_set_text(titulo, LV_SYMBOL_WIFI "  REDES");
  lv_obj_set_style_text_color(titulo, COR_BRANCO, 0);
  lv_obj_set_style_text_font(titulo, &lv_font_montserrat_20, 0);
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, 10, 0);

  Serial.println("Escaneando...");
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  Serial.printf("%d redes\n", n);

  logMemoria("depois scan WiFi");

  lv_obj_t * list = lv_obj_create(lv_scr_act());
  lv_obj_set_size(list, LARGURA - 10, ALTURA - 60);
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_bg_color(list, COR_FUNDO_1, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_style_pad_all(list, 5, 0);

  if (n == 0) {
    lv_obj_t * lbl = lv_label_create(list);
    lv_label_set_text(lbl, "Nenhuma rede encontrada");
    lv_obj_set_style_text_color(lbl, COR_BRANCO, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);

      if (ssid.length() == 0) continue;

      lv_obj_t * btn = lv_btn_create(list);
      lv_obj_set_size(btn, 220, 45);
      lv_obj_set_style_bg_color(btn, COR_CARD, 0);
      lv_obj_set_style_radius(btn, 8, 0);
      lv_obj_set_style_border_width(btn, 1, 0);
      lv_obj_set_style_border_color(btn, COR_BORDA, 0);

      char * ssid_copy = (char *) malloc(ssid.length() + 1);
      strcpy(ssid_copy, ssid.c_str());
      lv_obj_set_user_data(btn, ssid_copy);

      lv_obj_t * icone = lv_label_create(btn);
      lv_label_set_text(icone, LV_SYMBOL_WIFI);
      if (rssi > -60) lv_obj_set_style_text_color(icone, COR_VERDE, 0);
      else if (rssi > -75) lv_obj_set_style_text_color(icone, COR_AMARELO, 0);
      else lv_obj_set_style_text_color(icone, COR_VERMELHO, 0);
      lv_obj_set_style_text_font(icone, &lv_font_montserrat_14, 0);
      lv_obj_align(icone, LV_ALIGN_LEFT_MID, 8, 0);

      lv_obj_t * lbl = lv_label_create(btn);
      lv_label_set_text(lbl, ssid.c_str());
      lv_obj_set_style_text_color(lbl, COR_BRANCO, 0);
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
      lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 32, 0);

      lv_obj_t * lbl_rssi = lv_label_create(btn);
      lv_label_set_text_fmt(lbl_rssi, "%d", rssi);
      lv_obj_set_style_text_color(lbl_rssi, COR_CINZA, 0);
      lv_obj_set_style_text_font(lbl_rssi, &lv_font_montserrat_14, 0);
      lv_obj_align(lbl_rssi, LV_ALIGN_RIGHT_MID, -8, 0);

      lv_obj_add_event_cb(btn, rede_selecionada_event, LV_EVENT_CLICKED, NULL);
    }
  }

  WiFi.scanDelete();
  logMemoria("apos scanDelete");
}

// ========== TELA PRINCIPAL ==========
void criarTelaPrincipal() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), COR_FUNDO_1, 0);

  lv_obj_t * header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, LARGURA, 45);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_0, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t * btn_config = lv_btn_create(header);
  lv_obj_set_size(btn_config, 110, 32);
  lv_obj_align(btn_config, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_bg_color(btn_config, COR_AMARELO, 0);
  lv_obj_set_style_radius(btn_config, 8, 0);
  lv_obj_set_style_border_width(btn_config, 0, 0);

  lv_obj_t * lbl_btn = lv_label_create(btn_config);
  lv_label_set_text(lbl_btn, LV_SYMBOL_SETTINGS "  CONFIG");
  lv_obj_set_style_text_color(lbl_btn, lv_color_black(), 0);
  lv_obj_set_style_text_font(lbl_btn, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_btn);
  lv_obj_add_event_cb(btn_config, btn_config_event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * wifi_status = lv_label_create(header);
  lv_label_set_text(wifi_status, WiFi.status() == WL_CONNECTED ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING);
  lv_obj_set_style_text_color(wifi_status, WiFi.status() == WL_CONNECTED ? COR_VERDE : COR_VERMELHO, 0);
  lv_obj_set_style_text_font(wifi_status, &lv_font_montserrat_20, 0);
  lv_obj_align(wifi_status, LV_ALIGN_RIGHT_MID, -10, 0);

  lv_obj_t * card = lv_obj_create(lv_scr_act());
  lv_obj_set_size(card, 220, 265);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_set_style_bg_color(card, COR_CARD_BG, 0);
  lv_obj_set_style_border_color(card, COR_BORDA, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_shadow_width(card, 10, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
  lv_obj_set_scroll_dir(card, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t * content = lv_obj_create(card);
  lv_obj_set_size(content, 200, LV_SIZE_CONTENT);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_0, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_style_pad_all(content, 8, 0);

  lv_obj_t * avatar = lv_obj_create(content);
  lv_obj_set_size(avatar, 70, 70);
  lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(avatar, COR_AVATAR, 0);
  lv_obj_set_style_border_color(avatar, COR_DESTAQUE, 0);
  lv_obj_set_style_border_width(avatar, 3, 0);

  String iniciais = "";
  if (perfil.name.length() > 0) {
    iniciais += perfil.name.charAt(0);
    int espaco = perfil.name.indexOf(' ');
    if (espaco > 0 && espaco < perfil.name.length() - 1)
      iniciais += perfil.name.charAt(espaco + 1);
  } else if (perfil.login.length() > 0) {
    iniciais = perfil.login.substring(0, 2);
  }
  iniciais.toUpperCase();

  lv_obj_t * label_avatar = lv_label_create(avatar);
  lv_label_set_text(label_avatar, iniciais.c_str());
  lv_obj_set_style_text_color(label_avatar, COR_BRANCO, 0);
  lv_obj_set_style_text_font(label_avatar, &lv_font_montserrat_20, 0);
  lv_obj_center(label_avatar);

  lv_obj_t * label_nome = lv_label_create(content);
  lv_label_set_text(label_nome, perfil.name.length() > 0 ? perfil.name.c_str() : perfil.login.c_str());
  lv_obj_set_style_text_color(label_nome, COR_BRANCO, 0);
  lv_obj_set_style_text_font(label_nome, &lv_font_montserrat_14, 0);
  lv_obj_set_width(label_nome, 190);
  lv_label_set_long_mode(label_nome, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(label_nome, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * label_login = lv_label_create(content);
  lv_label_set_text(label_login, ("@" + perfil.login).c_str());
  lv_obj_set_style_text_color(label_login, COR_DESTAQUE, 0);
  lv_obj_set_style_text_font(label_login, &lv_font_montserrat_14, 0);

  lv_obj_t * linha = lv_obj_create(content);
  lv_obj_set_size(linha, 180, 1);
  lv_obj_set_style_bg_color(linha, COR_BORDA, 0);
  lv_obj_set_style_border_width(linha, 0, 0);

  lv_obj_t * label_bio = lv_label_create(content);
  lv_label_set_text(label_bio, perfil.bio.length() > 0 ? perfil.bio.c_str() : "Sem bio");
  lv_obj_set_style_text_color(label_bio, COR_CINZA, 0);
  lv_obj_set_style_text_font(label_bio, &lv_font_montserrat_14, 0);
  lv_obj_set_width(label_bio, 190);
  lv_label_set_long_mode(label_bio, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(label_bio, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t * label_langs = lv_label_create(content);
  lv_label_set_text(label_langs, "LINGUAGENS");
  lv_obj_set_style_text_color(label_langs, COR_DESTAQUE, 0);
  lv_obj_set_style_text_font(label_langs, &lv_font_montserrat_14, 0);

  for (int i = 0; i < numLinguagens; i++) {
    lv_obj_t * lang = lv_label_create(content);
    lv_label_set_text_fmt(lang, "%s  (%ld KB)", linguagens[i].nome.c_str(), linguagens[i].bytes / 1024);
    lv_obj_set_style_text_color(lang, COR_BRANCO, 0);
    lv_obj_set_style_text_font(lang, &lv_font_montserrat_14, 0);
  }

  lv_obj_t * linha2 = lv_obj_create(content);
  lv_obj_set_size(linha2, 180, 1);
  lv_obj_set_style_bg_color(linha2, COR_BORDA, 0);
  lv_obj_set_style_border_width(linha2, 0, 0);

  lv_obj_t * stats = lv_obj_create(content);
  lv_obj_set_size(stats, 190, 35);
  lv_obj_set_style_bg_opa(stats, LV_OPA_0, 0);
  lv_obj_set_style_border_width(stats, 0, 0);
  lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t * stat1 = lv_obj_create(stats);
  lv_obj_set_size(stat1, 85, 35);
  lv_obj_set_style_bg_color(stat1, lv_color_hex(0x3186), 0);
  lv_obj_set_style_border_width(stat1, 0, 0);
  lv_obj_set_style_radius(stat1, 6, 0);

  lv_obj_t * ico1 = lv_label_create(stat1);
  lv_label_set_text(ico1, LV_SYMBOL_DIRECTORY);
  lv_obj_set_style_text_color(ico1, COR_DESTAQUE, 0);
  lv_obj_set_style_text_font(ico1, &lv_font_montserrat_14, 0);
  lv_obj_align(ico1, LV_ALIGN_LEFT_MID, 5, 0);

  lv_obj_t * lbl_rep = lv_label_create(stat1);
  lv_label_set_text_fmt(lbl_rep, "%d", perfil.publicRepos);
  lv_obj_set_style_text_color(lbl_rep, COR_BRANCO, 0);
  lv_obj_set_style_text_font(lbl_rep, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl_rep, LV_ALIGN_RIGHT_MID, -5, 0);

  lv_obj_t * stat2 = lv_obj_create(stats);
  lv_obj_set_size(stat2, 85, 35);
  lv_obj_set_style_bg_color(stat2, lv_color_hex(0x3186), 0);
  lv_obj_set_style_border_width(stat2, 0, 0);
  lv_obj_set_style_radius(stat2, 6, 0);

  lv_obj_t * ico2 = lv_label_create(stat2);
  lv_label_set_text(ico2, LV_SYMBOL_EYE_OPEN);
  lv_obj_set_style_text_color(ico2, COR_DESTAQUE, 0);
  lv_obj_set_style_text_font(ico2, &lv_font_montserrat_14, 0);
  lv_obj_align(ico2, LV_ALIGN_LEFT_MID, 5, 0);

  lv_obj_t * lbl_fol = lv_label_create(stat2);
  lv_label_set_text_fmt(lbl_fol, "%d", perfil.followers);
  lv_obj_set_style_text_color(lbl_fol, COR_BRANCO, 0);
  lv_obj_set_style_text_font(lbl_fol, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl_fol, LV_ALIGN_RIGHT_MID, -5, 0);

  lv_obj_t * linha3 = lv_obj_create(content);
  lv_obj_set_size(linha3, 180, 1);
  lv_obj_set_style_bg_color(linha3, COR_BORDA, 0);
  lv_obj_set_style_border_width(linha3, 0, 0);

  String urlPerfil = "https://github.com/" + perfil.login;
  lv_obj_t * qr = lv_qrcode_create(content, 80, lv_color_hex(0x0000), lv_color_hex(0xFFFF));
  lv_qrcode_update(qr, urlPerfil.c_str(), urlPerfil.length());
}

// ========== CALLBACK BOTÃO CONFIG ==========
static void btn_config_event(lv_event_t * e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  telaAtual = TELA_LISTA_WIFI;
  criarTelaListaWiFi();
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(500);
  logMemoria("inicio setup");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  touchSPI.begin(25, 39, 32, 33);
  ts.begin(touchSPI);
  ts.setRotation(0);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LARGURA * 6);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LARGURA;
  disp_drv.ver_res = ALTURA;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  logMemoria("depois LVGL init");

  carregarConfig();

  if (cfg_ssid.length() > 0 && cfg_github.length() > 0) {
    client.setInsecure();
    criarTelaLoading("Conectando WiFi...");
    if (conectarWiFi()) {
      criarTelaLoading("Buscando GitHub...");
      if (buscarGitHub()) {
        criarTelaLoading("Buscando linguagens...");
        buscarLinguagens();

        telaAtual = TELA_PRINCIPAL;
        criarTelaPrincipal();
        lastRefresh = millis();
        logMemoria("fim setup");
        return;
      }
    }
  }

  telaAtual = TELA_LISTA_WIFI;
  criarTelaListaWiFi();
  logMemoria("fim setup (config)");
}

// ========== LOOP ==========
void loop() {
  lv_timer_handler();

  if (telaAtual == TELA_PRINCIPAL && millis() - lastRefresh > REFRESH_INTERVAL) {
    lastRefresh = millis();
    Serial.println("Atualizando dados...");
    if (buscarGitHub()) {
      buscarLinguagens();
      criarTelaPrincipal();
    }
  }

  yield();
  delay(5);
}