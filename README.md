# MSX2 para VGA32 (RobGo-RG)

Emulador MSX2 (fMSX 6.0 de Marat Fayzullin) rodando direto no ESP32-WROVER da TTGO VGA32 / RobGo-RG.
Sem launcher, sem Retro-Go, sem ESP-IDF externo: um projeto PlatformIO normal (Arduino + FabGL).

## Compilar

Abra a pasta no VS Code (extensão PlatformIO) e use **Build** e **Upload**. O `platformio.ini` já tem tudo.

## Cartão SD

Formato FAT32. Coloque a BIOS em `/MSX2` 

| Arquivo | Uso |
|---|---|
| `MSX2.ROM` (32 KB) | obrigatório |
| `MSX2EXT.ROM` (16 KB) | obrigatório |
| `DISK.ROM` | necessário para disquetes |
| `MSXDOS2.ROM` | opcional (MSX-DOS 2) |

A BIOS não vem no projeto. Os nomes precisam ser exatamente estes.
ROMs, disquetes (`.dsk`) e estados (`.sta`) são escolhidos pelo menu do fMSX (F10).

## Teclas (teclado PS/2)

| Tecla | Ação |
|---|---|
| F10 | menu do fMSX: abrir arquivo, salvar estado, modelo, discos, reset, reboot |
| F12 | reset do MSX |
| F11 | alterna frame skip (0 / 25 / 50 / 75 %) |
| F1–F5 | F1–F5 do MSX |
| Setas + Espaço/Enter + Ctrl/Esc | também funcionam como joystick 1 |
| Mouse PS/2 | mouse do MSX |

## /sd/bootl.rc (opcional)

Arquivo de texto UTF-8, uma linha `chave=valor`; `#` e `;` comentam.

| Chave | Efeito |
|---|---|
| `kbddat=27` e `kbdclk=26` | teclado na porta 27/26 (mouse em 32/33). Sem isto: teclado em 32/33 |
| `msx_biosdir=/msx` | pasta da BIOS |
| `msx_rom=/msx/jogo.rom` | cartucho carregado ao ligar |
| `msx_diska=/msx/disco.dsk`, `msx_diskb=...` | disquetes carregados ao ligar |
| `msx_frameskip=50` | quadros não desenhados: 0, 25, 50 ou 75 |
| `msx_autoskip=1` | ajuste automático do frame skip |
| `msx_fm=1` | síntese FM (YM2413) ligada/desligada |

## Pinos

Todos em `src/board.h`. O MISO do SD é tentado em GPIO35 e depois em GPIO2 (nas duas velocidades: 20, 10 e 4 MHz).
Áudio: DAC interno, GPIO25 (mono).

## Estrutura

```
src/main.cpp        setup(): SD, vídeo, teclado, áudio; emulador no núcleo 0
src/msx_glue.c      cola em C entre o fMSX e o hardware (vídeo, teclas, áudio, frame skip)
src/hal.h           fronteira C <-> hardware
src/hal_*.cpp       SD/bootl.rc, VGA (FabGL), PS/2 (FabGL), áudio (I2S/DAC)
src/fmsx/           núcleo fMSX (EMULib, fMSX, Z80) + msxfix.c/h (cwd no ESP32)
```

O núcleo tem as alterações do fork robgo (mixer com SCC e FM, soft-clip, Z80 na IRAM, menu enxuto,
navegação de pastas), marcadas com `RG_TARGET_ROBGO_RG`.

## Se algo falhar

- **Linker reclama de `iram0_0_seg`**: em `platformio.ini`, troque `-DMSX_Z80_IN_IRAM=1` por `0`.
- **Tela azul com mensagem**: SD ausente ou BIOS não encontrada. O monitor serial (115200) mostra os detalhes.

## Licença do fMSX

O fMSX não pode ser distribuído comercialmente (ver cabeçalhos dos arquivos em `src/fmsx`).
