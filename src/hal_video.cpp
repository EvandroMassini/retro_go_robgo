// hal_video.cpp - saída VGA 320x240 (64 cores) pela FabGL
#include <Arduino.h>
#include <string.h>
#include "fabgl.h"
#include "hal.h"
#include "board.h"

using namespace fabgl;

static VGAController video;
static uint8_t lut_r[32], lut_g[64], lut_b[32], sync_bits;
static int vw, vh;
static bool video_ready;

extern "C" void hal_video_init(void)
{
    video.begin((gpio_num_t)PIN_VGA_RED1, (gpio_num_t)PIN_VGA_RED0,
                (gpio_num_t)PIN_VGA_GREEN1, (gpio_num_t)PIN_VGA_GREEN0,
                (gpio_num_t)PIN_VGA_BLUE1, (gpio_num_t)PIN_VGA_BLUE0,
                (gpio_num_t)PIN_VGA_HSYNC, (gpio_num_t)PIN_VGA_VSYNC);
    // Linhas repetidas por DMA (double scan): 320x240 ocupam a tela toda sem cópia 2x2.
    video.setResolution(QVGA_320x240_60Hz, 320, 240, false);
    vw = video.getViewPortWidth();
    vh = video.getViewPortHeight();
    hal_log("VGA: %dx%d\n", vw, vh);

    // Tabelas RGB565 -> pixel bruto da FabGL (2 bits por cor + bits de sincronismo).
    sync_bits = video.createRawPixel(RGB222(0, 0, 0));
    for (unsigned i = 0; i < 32; ++i) {
        unsigned level = (i * 3 + 15) / 31;
        lut_r[i] = video.createRawPixel(RGB222(level, 0, 0)) ^ sync_bits;
        lut_b[i] = video.createRawPixel(RGB222(0, 0, level)) ^ sync_bits;
    }
    for (unsigned i = 0; i < 64; ++i)
        lut_g[i] = video.createRawPixel(RGB222(0, (i * 3 + 31) / 63, 0)) ^ sync_bits;

    for (int y = 0; y < vh; ++y)
        memset(video.getScanline(y), sync_bits, vw);
    video_ready = true;
}

// pixels: RGB565 com os bytes trocados. O ^2 no índice é a ordem de bytes do DMA da FabGL.
extern "C" void hal_video_present(const uint16_t *pixels, int w, int h, int stride)
{
    if (!video_ready) return;
    const int x0 = (vw - w) / 2;
    const int y0 = (vh - h) / 2;
    const int first = x0 < 0 ? -x0 : 0;               // recorta se a alocação vier menor
    const int last = x0 + w > vw ? vw - x0 : w;
    for (int y = 0; y < h; ++y) {
        const int py = y0 + y;
        if (py < 0 || py >= vh) continue;
        uint8_t *dst = video.getScanline(py);
        const uint16_t *src = pixels + y * stride;
        for (int x = first; x < last; ++x) {
            const uint16_t c = __builtin_bswap16(src[x]);
            dst[(x0 + x) ^ 2] = sync_bits | lut_r[c >> 11] | lut_g[(c >> 5) & 63] | lut_b[c & 31];
        }
    }
}

// Mensagem de erro na tela (só usada antes de o emulador começar).
static void draw_wrapped(fabgl::Canvas &cv, const char *text, int &y)
{
    const int cols = 38;                               // fonte 8x8 em 320 px, com margem
    char line[cols + 1];
    while (*text) {
        int n = (int)strlen(text);
        if (n > cols) {
            n = cols;
            while (n > 0 && text[n] != ' ') --n;
            if (n == 0) n = cols;
        }
        memcpy(line, text, n);
        line[n] = 0;
        cv.drawText(8, y, line);
        y += 12;
        text += n;
        while (*text == ' ') ++text;
    }
}

extern "C" void hal_screen_message(const char *l1, const char *l2)
{
    hal_log("%s\n%s\n", l1, l2 ? l2 : "");
    if (!video_ready) return;
    fabgl::Canvas cv(&video);
    cv.setBrushColor(fabgl::Color::Blue);
    cv.clear();
    cv.setPenColor(fabgl::Color::BrightWhite);
    cv.selectFont(&fabgl::FONT_8x8);
    int y = 96;
    draw_wrapped(cv, l1, y);
    y += 8;
    if (l2) draw_wrapped(cv, l2, y);
}
