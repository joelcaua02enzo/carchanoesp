# 🎫 Crachá GitHub com ESP32-S3

Crachá digital que exibe suas informações do GitHub em uma tela TFT 2.8" com touch. Conecta-se ao Wi-Fi, consulta a API do GitHub e mostra perfil, estatísticas, linguagens mais usadas e QR Code para o seu perfil.

![Status](https://img.shields.io/badge/status-funcionando-brightgreen)
![Plataforma](https://img.shields.io/badge/plataforma-ESP32--S3-blue)
![LVGL](https://img.shields.io/badge/LVGL-v8.x-orange)
![Licença](https://img.shields.io/badge/licen%C3%A7a-MIT-green)

---

## 📸 Preview

<img width="899" height="1599" alt="WhatsApp Image 2026-09-23 at 15 22 29" src="https://github.com/user-attachments/assets/af5324f6-df9e-4701-a16d-6c0090c641b2" />

---

## ✨ Funcionalidades

- 🔍 Escaneia redes Wi-Fi disponíveis e mostra numa lista com ícone de sinal
- 🔑 Teclado virtual para digitar a senha do Wi-Fi e o usuário do GitHub
- 💾 Salva configurações na memória flash (Preferences) — não precisa recompilar
- 📊 Exibe dados do GitHub: nome, usuário, bio, localização, seguidores, repositórios
- 🏆 Top 5 linguagens mais usadas (baseado nos bytes de código)
- 📱 QR Code com link direto para o seu perfil
- 🎨 Interface moderna com cartão, sombras, cores e ícones
- 📜 Scroll vertical para navegar pelas informações
- 🔄 Atualização automática a cada 5 minutos (configurável)
- 🔐 Suporte a Personal Access Token do GitHub (aumenta limite de 60 para 5000 req/hora)

---

## 🛠️ Hardware Necessário

| Componente | Especificação |
|-----------|---------------|
| Placa | ESP32-S3 com tela TFT 2.8" integrada |
| Display | ILI9341 (240x320) |
| Touch | XPT2046 (resistivo) |
| Backlight | Controlado por GPIO |
| Conexão | USB-C |

### Pinagem (conforme o módulo)

| Pino do Display | GPIO do ESP32-S3 |
|-----------------|------------------|
| TFT_MOSI (SDA)  | 13 |
| TFT_SCLK (SCK)  | 14 |
| TFT_CS          | 15 |
| TFT_DC          | 2 |
| TFT_RST         | 12 |
| TFT_BL          | 21 |
| TOUCH_CS        | 33 |
| TOUCH_CLK       | 25 |
| TOUCH_DIN       | 32 |
| TOUCH_OUT       | 39 |
| TOUCH_IRQ       | 36 |

---

## 📚 Bibliotecas Necessárias

Instale via Gerenciador de Bibliotecas da Arduino IDE:

| Biblioteca | Versão | Autor |
|-----------|--------|-------|
| TFT_eSPI | 2.5.x | Bodmer |
| XPT2046_Touchscreen | 1.4.0 | Paul Stoffregen |
| ArduinoJson | 6.x | Benoit Blanchon |
| LVGL | 8.4.0 ou 8.3.9 | LVGL |

Importante: Use a versão 8.x do LVGL. A v9 tem API diferente e pode não compilar.

---

## ⚙️ Configuração

### 1. Clonar o repositório

```bash
git clone https://github.com/SEU_USUARIO/esp32-github-cracha.git
cd esp32-github-cracha
```
## Substitua o conteúdo do arquivo libraries/TFT_eSPI/User_Setup.h por
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  12
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS 33

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  1000000

#define USE_HSPI_PORT

##Copie o arquivo lv_conf_template.h da pasta libraries/lvgl/ para libraries/lv_conf.h e ative:
#if 1   // era #if 0
#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (48U * 1024U)
#define LV_TICK_CUSTOM 1
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1

#define LV_USE_QRCODE 1

(Opcional) Personal Access Token do GitHub
Para aumentar o limite de requisições de 60/hora para 5000/hora:

Acesse https://github.com/settings/tokens

Clique em Generate new token (classic)

Marque o escopo public_repo

Copie o token gerado

Cole no código, na linha:
````
const char* githubToken = "ghp_xxxxxxxxxxxxxxxxxxxx";
````

👤 Autor
Joel caua

GitHub: https://github.com/joelcaua02enzo

Email: joel@email.com
