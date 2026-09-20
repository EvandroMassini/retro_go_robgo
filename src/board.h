// board.h - pinos da placa (TTGO VGA32 v1.4 / RobGo-RG, ESP32-WROVER com PSRAM)
#pragma once

// VGA. A FabGL recebe primeiro o bit alto e depois o baixo de cada cor.
#define PIN_VGA_RED1    22
#define PIN_VGA_RED0    21
#define PIN_VGA_GREEN1  19
#define PIN_VGA_GREEN0  18
#define PIN_VGA_BLUE1    5
#define PIN_VGA_BLUE0    4
#define PIN_VGA_HSYNC   23
#define PIN_VGA_VSYNC   15

// PS/2. Perfil padrão: teclado em 32/33 e mouse em 27/26.
// Perfil alternativo (bootl.rc: kbddat=27 / kbdclk=26): teclado em 27/26 e mouse em 32/33.
#define PIN_PS2_A_DATA  32
#define PIN_PS2_A_CLK   33
#define PIN_PS2_B_DATA  27
#define PIN_PS2_B_CLK   26

// Cartão SD (SPI). CS, CLK e MOSI são iguais nas duas placas; o MISO muda:
// RobGo = GPIO35, LilyGO = GPIO2. O firmware tenta os dois, na ordem abaixo.
#define PIN_SD_CS       13
#define PIN_SD_MOSI     12
#define PIN_SD_CLK      14
#ifndef PIN_SD_MISO_A
#define PIN_SD_MISO_A   35
#endif
#ifndef PIN_SD_MISO_B
#define PIN_SD_MISO_B    2
#endif

// Áudio: DAC interno do ESP32, canal direito = GPIO25 (mono).
