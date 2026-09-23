<img width="899" height="1599" alt="image" src="https://github.com/user-attachments/assets/ef4c7511-b5f1-4991-b999-2ebdc45ccbb4" />

# Crachá GitHub com ESP32-S3

Crachá digital que exibe suas informações do GitHub em uma tela TFT 2.8 polegadas com touch. Conecta-se ao Wi-Fi, consulta a API do GitHub e mostra perfil, estatísticas, linguagens mais usadas e QR Code para o seu perfil.

Status: funcionando
Plataforma: ESP32-S3
LVGL: v8.x
Licença: MIT

## Preview

O crachá mostra na tela um cartão com avatar, nome, usuário, bio, linguagens mais usadas, número de repositórios, seguidores e um QR Code que leva direto para o seu perfil do GitHub. O botão CONFIG fica no topo esquerdo e o status do Wi-Fi no topo direito. Você pode arrastar o dedo dentro do cartão para ver todas as informações.

## Funcionalidades

O projeto escaneia redes Wi-Fi disponíveis e mostra numa lista com ícone de sinal. Tem um teclado virtual para digitar a senha do Wi-Fi e o usuário do GitHub. Salva as configurações na memória flash usando Preferences, então não precisa recompilar toda vez que quiser trocar de rede ou usuário. Exibe dados do GitHub como nome, usuário, bio, localização, seguidores e número de repositórios públicos. Mostra as top 5 linguagens mais usadas com base nos bytes de código dos seus repositórios. Gera um QR Code com link direto para o seu perfil. A interface é moderna, com cartão, sombras, cores e ícones. Tem scroll vertical para navegar pelas informações. Atualiza automaticamente a cada 5 minutos, mas você pode configurar esse intervalo. Também suporta Personal Access Token do GitHub, o que aumenta o limite de requisições de 60 por hora para 5000 por hora.

## Hardware Necessário

Você vai precisar de uma placa ESP32-S3 com tela TFT 2.8 polegadas integrada, display ILI9341 de 240x320 pixels, touch XPT2046 resistivo, backlight controlado por GPIO e conexão USB-C.

A pinagem conforme o módulo é: TFT_MOSI no GPIO 13, TFT_SCLK no GPIO 14, TFT_CS no GPIO 15, TFT_DC no GPIO 2, TFT_RST no GPIO 12, TFT_BL no GPIO 21, TOUCH_CS no GPIO 33, TOUCH_CLK no GPIO 25, TOUCH_DIN no GPIO 32, TOUCH_OUT no GPIO 39 e TOUCH_IRQ no GPIO 36.

## Bibliotecas Necessárias

Instale via Gerenciador de Bibliotecas da Arduino IDE: TFT_eSPI versão 2.5.x do Bodmer, XPT2046_Touchscreen versão 1.4.0 do Paul Stoffregen, ArduinoJson versão 6.x do Benoit Blanchon e LVGL versão 8.4.0 ou 8.3.9. É importante usar a versão 8.x do LVGL, porque a v9 tem API diferente e pode não compilar.

## Configuração

Primeiro clone o repositório com o comando git clone https://github.com/SEU_USUARIO/esp32-github-cracha.git e entre na pasta com cd esp32-github-cracha.

Depois configure o arquivo User_Setup.h da TFT_eSPI. Substitua o conteúdo do arquivo libraries/TFT_eSPI/User_Setup.h pelas definições do driver ILI9341, largura 240, altura 320, pinos MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST 12, BL 21, backlight ativo em HIGH, TOUCH_CS 33, fontes GLCD, FONT2, FONT4, FONT6, FONT7, FONT8, GFXFF, SMOOTH_FONT, frequência SPI de 40 MHz, frequência de leitura de 20 MHz, frequência do touch de 1 MHz e USE_HSPI_PORT ativado.

Depois configure o arquivo lv_conf.h. Copie o arquivo lv_conf_template.h da pasta libraries/lvgl/ para libraries/lv_conf.h e ative o conteúdo mudando #if 0 para #if 1. Defina LV_COLOR_DEPTH como 16, LV_MEM_SIZE como 48 vezes 1024 bytes, LV_TICK_CUSTOM como 1, LV_USE_LOG como 0, LV_USE_PERF_MONITOR como 0, LV_FONT_MONTSERRAT_14 como 1, LV_FONT_MONTSERRAT_20 como 1 e LV_USE_QRCODE como 1.

Opcionalmente, você pode usar um Personal Access Token do GitHub para aumentar o limite de requisições de 60 por hora para 5000 por hora. Acesse https://github.com/settings/tokens, clique em Generate new token (classic), marque o escopo public_repo, copie o token gerado e cole no código na linha const char* githubToken.

Na Arduino IDE, configure a placa como ESP32S3 Dev Module, Flash Mode DIO, Flash Size 4MB (32Mb), PSRAM OPI PSRAM se disponível, Partition Scheme Default 4MB with spiffs e Upload Speed 921600. Depois abra o arquivo .ino e clique em Upload.

## Como Usar

Na primeira inicialização, o ESP32 escaneia as redes Wi-Fi disponíveis. Toque na rede que deseja conectar, digite a senha com o teclado virtual e toque no botão de confirmação. Depois digite seu usuário do GitHub e toque em confirmar. O ESP32 salva tudo e já exibe o crachá.

No uso diário, o ESP32 conecta automaticamente e busca seus dados. Arraste dentro do cartão para ver todas as informações. Toque em CONFIG para reconfigurar Wi-Fi ou usuário. Os dados são atualizados automaticamente a cada 5 minutos.

## Estrutura do Projeto

A estrutura é uma pasta principal chamada esp32-github-cracha contendo o arquivo esp32-github-cracha.ino que é o sketch principal, o arquivo README.md que é este documento, o arquivo LICENSE com a licença MIT e uma pasta docs com preview.png para screenshot e esquema.png para diagrama de ligação.

## Como Funciona

O ESP32-S3 com tela TFT e touch se conecta via HTTPS à API REST do GitHub. A API retorna JSON, que é processado com ArduinoJson. A interface gráfica é renderizada com LVGL na tela TFT. A conexão é feita via Wi-Fi. As configurações são salvas na flash usando NVS (Preferences). O fluxo é: Wi-Fi conecta à rede configurada, API GitHub consulta os endpoints /users/{username} e /users/{username}/repos, o processamento extrai os dados com ArduinoJson, a exibição renderiza com LVGL na tela TFT e a persistência salva SSID, senha e usuário.

## Solução de Problemas

Se aparecer erro HTTP 403, a causa é limite de requisições excedido ou token inválido. A solução é usar um Personal Access Token, aumentar o REFRESH_INTERVAL para 10 minutos ou aguardar 1 hora se o limite foi excedido.

Se a tela ficar branca ou preta, a causa é User_Setup.h incorreto. Verifique se o driver é ILI9341_DRIVER e se a resolução é 240x320.

Se o touch não responder, a causa é barramento SPI conflitante. Adicione #define USE_HSPI_PORT no User_Setup.h.

Se aparecer o erro undefined reference to setup(), a causa é código incompleto ou chaves faltando. Certifique-se de que o arquivo é .ino e está completo.

Se o LVGL não compilar, a causa é versão incorreta. Use LVGL 8.4.0 ou 8.3.9. A v9 tem API diferente.

## Contribuindo

Contribuições são bem-vindas. Faça um fork do projeto, crie uma branch para sua feature, faça commit das mudanças, faça push para a branch e abra um Pull Request.

## Licença

Este projeto está sob a licença MIT. Veja o arquivo LICENSE para mais detalhes.

## Autor

joel caua
GitHub: https://github.com/joelcaua02enzo
Email: joel@email.com

## Agradecimentos

Agradecimentos a Bodmer pela biblioteca TFT_eSPI, a Paul Stoffregen pela XPT2046_Touchscreen, a Benoit Blanchon pela ArduinoJson, à LVGL pela biblioteca gráfica e ao GitHub pela API REST.

## Se este projeto te ajudou, deixe uma estrela!

https://github.com/joelcaua02enzo/esp32-github-cracha
