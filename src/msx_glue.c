// msx_glue.c - liga o núcleo fMSX (Marat Fayzullin) ao hardware através do hal.h.
// Aqui ficam as funções que o fMSX espera que a plataforma forneça:
// vídeo, teclado/joystick, mouse, áudio e o ritmo de execução (frame skip).
#include "msxfix.h"
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Tela do MSX: 256 x 228 (192 ou 212 linhas + bordas), RGB565, telas 512 px "estreitas".
#define BPP16
#define WIDTH 256
#define HEIGHT 228

// O framebuffer é RGB565 com os bytes trocados (big-endian): é o formato que o Menu.c do
// fork usa nos seus PIXEL(). O hal_video_present desfaz a troca ao converter para VGA.
#define C_RGB(R, G, B) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xFF) >> 3))
#define SWAP16(C) ((uint16_t)((((C) >> 8) | ((C) << 8)) & 0xFFFF))

static uint16_t BPal[256];
static uint16_t XPal[80];
static uint16_t XPal0;
static uint16_t *XBuf;

void PutImage(void);   // usado pelo RefreshScreen() do Common.h

#include "fmsx.h"

const char *Title = "fMSX 6.0";
const char *Disks[2][MAXDISKS + 1];

static uint16_t *FrameBuf;
static Image NormScreen;

static int JoyState, LastKey, InMenu;
static uint32_t PrevNav;

// ---------------------------------------------------------------------------
// Ritmo de execução. O áudio dita a velocidade (WriteAudio bloqueia quando o
// buffer enche). Se o emulador não consegue acompanhar, pulamos mais quadros.
// ---------------------------------------------------------------------------
static int FrameSkip = 50;            // % de quadros NÃO desenhados (0, 25, 50, 75)
static int EffectiveFrameSkip = 50;
static int AutoFrameSkip = 1;
static unsigned drawn_frames;         // quadros desenhados na janela do frame skip
// Contadores lidos pela tarefa de diagnóstico do main.cpp (contam desde o boot).
static volatile unsigned stat_emu_frames, stat_drawn_frames;
static volatile const char *stat_where = "boot";
static int64_t control_start;
static unsigned control_frames, recovery_windows;

static void reset_frame_timing(void)
{
    control_start = 0;
    control_frames = recovery_windows = 0;
    drawn_frames = 0;
}

static void apply_frame_skip(void)
{
    EffectiveFrameSkip = FrameSkip;
    UPeriod = 100 - FrameSkip;
}

static void pace_control(void)
{
    const int target = PALVideo ? 50 : 60;
    const unsigned full = HAL_AUDIO_STREAM_SAMPLES * sizeof(int16_t);
    int64_t now = hal_time_us();

    if (!control_start) control_start = now;
    ++control_frames;
    if (now - control_start < 1000000) return;

    double rate = control_frames * 1000000.0 / (double)(now - control_start);
    unsigned queued = hal_audio_queued_bytes();

    if (AutoFrameSkip && now - control_start < 2000000) {
        if (rate < target * 0.97 && queued < full / 8) {
            // Atrasado e sem áudio na fila: desenha menos.
            EffectiveFrameSkip = EffectiveFrameSkip + 5 > 75 ? 75 : EffectiveFrameSkip + 5;
            recovery_windows = 0;
        } else if (rate >= target * 0.98 && queued >= full / 2) {
            // Folga por 10 s seguidos: volta a desenhar mais.
            if (++recovery_windows >= 10) {
                EffectiveFrameSkip = EffectiveFrameSkip - 5 < FrameSkip ? FrameSkip : EffectiveFrameSkip - 5;
                recovery_windows = 0;
            }
        } else {
            recovery_windows = 0;
        }
    } else {
        recovery_windows = 0;
        if (!AutoFrameSkip) EffectiveFrameSkip = FrameSkip;
    }
    UPeriod = 100 - EffectiveFrameSkip;
    control_start = now;
    control_frames = 0;
    drawn_frames = 0;
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
#define NAV_MASK (HAL_NAV_UP | HAL_NAV_DOWN | HAL_NAV_LEFT | HAL_NAV_RIGHT | HAL_NAV_A | HAL_NAV_B)

static void wait_nav_release(int max_ms)
{
    for (int waited = 0; (hal_input_nav() & NAV_MASK) && waited < max_ms; waited += 10)
        hal_delay_ms(10);
}

int ProcessEvents(int Wait)
{
    (void)Wait;
    // Matriz do teclado MSX: 0xFF = nenhuma tecla apertada.
    for (int i = 0; i < 16; ++i)
        KeyState[i] = 0xFF;
    JoyState = 0;

    uint8_t keys[256];
    hal_input_keys(keys);
    for (int k = 1; k < 130; ++k)
        if (keys[k]) { KBD_SET(k); }

    uint32_t nav = hal_input_nav();
    uint32_t rising = nav & ~PrevNav;
    PrevNav = nav;

    if (!InMenu) {
        if (rising & HAL_HOT_RESET) {
            ResetMSX(Mode, RAMPages, VRAMPages);
            reset_frame_timing();
            return 0;
        }
        if (rising & HAL_HOT_SKIP) {
            FrameSkip = (FrameSkip + 25) % 100;
            apply_frame_skip();
            hal_log("frame skip: %d%%\n", FrameSkip);
        }
        if (rising & HAL_HOT_MENU)
            InMenu = 2;
    }

    if (InMenu == 2) {
        // Menu do próprio fMSX: carrega cartucho/disco, salva estado, opções...
        InMenu = 1;
        stat_where = "menu";
        MenuMSX();
        wait_nav_release(500);
        PrevNav = hal_input_nav();
        InMenu = 0;
        reset_frame_timing();
        return 0;
    }

    if (InMenu) {
        // Dentro do menu, as setas/Enter/Esc viram teclas do console.
        if (nav & HAL_NAV_LEFT)  LastKey = CON_LEFT;
        if (nav & HAL_NAV_RIGHT) LastKey = CON_RIGHT;
        if (nav & HAL_NAV_UP)    LastKey = CON_UP;
        if (nav & HAL_NAV_DOWN)  LastKey = CON_DOWN;
        if (nav & HAL_NAV_A)     LastKey = CON_OK;
        if (nav & HAL_NAV_B)     LastKey = CON_EXIT;
    } else {
        // Fora do menu, as mesmas teclas também funcionam como joystick 1.
        if (nav & HAL_NAV_LEFT)  JoyState |= JST_LEFT;
        if (nav & HAL_NAV_RIGHT) JoyState |= JST_RIGHT;
        if (nav & HAL_NAV_UP)    JoyState |= JST_UP;
        if (nav & HAL_NAV_DOWN)  JoyState |= JST_DOWN;
        if (nav & HAL_NAV_A)     JoyState |= JST_FIREA;
        if (nav & HAL_NAV_B)     JoyState |= JST_FIREB;
    }
    return 0;
}

unsigned int Joystick(void)
{
    ProcessEvents(0);
    return JoyState;
}

void Keyboard(void)
{
    // O MSX lê a matriz diretamente: atualiza mesmo sem leitura de joystick.
    stat_where = "keyboard";
    ProcessEvents(0);
    ++stat_emu_frames;
    pace_control();
    stat_where = "run";
}

unsigned int Mouse(byte N)
{
    return N == 0 ? hal_input_mouse() : 0;
}

unsigned int GetJoystick(void)
{
    ProcessEvents(0);
    return 0;
}

unsigned int GetMouse(void)
{
    return 0;
}

unsigned int GetKey(void)
{
    unsigned int J;
    ProcessEvents(0);
    J = LastKey;
    LastKey = 0;
    return J;
}

unsigned int WaitKey(void)
{
    LastKey = 0;
    wait_nav_release(1000000);
    while (!(hal_input_nav() & NAV_MASK))
        hal_delay_ms(10);
    return GetKey();
}

unsigned int WaitKeyOrMouse(void)
{
    LastKey = WaitKey();
    return 0;
}

// ---------------------------------------------------------------------------
// Vídeo
// ---------------------------------------------------------------------------
int InitMachine(void)
{
    if (!FrameBuf)
        FrameBuf = (uint16_t *)hal_alloc_big(WIDTH * HEIGHT * sizeof(uint16_t));
    if (!FrameBuf)
        return 0;
    memset(FrameBuf, 0, WIDTH * HEIGHT * sizeof(uint16_t));

    XBuf = FrameBuf;
    NormScreen = (Image){
        .Data = FrameBuf,
        .W = WIDTH,
        .H = HEIGHT,
        .L = WIDTH,
        .D = 16,
    };
    SetScreenDepth(NormScreen.D);
    SetVideo(&NormScreen, 0, 0, WIDTH, HEIGHT);

    for (int J = 0; J < 80; J++)
        SetColor(J, 0, 0, 0);

    // Paleta fixa da SCREEN 8 (GRB 3-3-2).
    for (int J = 0; J < 256; J++)
        BPal[J] = SWAP16(C_RGB(((J >> 2) & 0x07) * 255 / 7, ((J >> 5) & 0x07) * 255 / 7, (J & 0x03) * 255 / 3));

    InitSound(HAL_AUDIO_RATE, 150);
    SetChannels(96, 0xFFFFFFFF);

    RPLInit(SaveState, LoadState, MAX_STASIZE);
    RPLRecord(RPL_RESET);
    reset_frame_timing();
    return 1;
}

void TrashMachine(void)
{
    RPLTrash();
    TrashSound();
}

void SetColor(byte N, byte R, byte G, byte B)
{
    uint16_t color = SWAP16(C_RGB(R, G, B));
    if (N)
        XPal[N] = color;
    else
        XPal0 = color;
}

void PutImage(void)
{
    ++drawn_frames;
    ++stat_drawn_frames;
    stat_where = "video";
    hal_video_present(FrameBuf, WIDTH, HEIGHT, WIDTH);
    stat_where = "run";
}

int ShowVideo(void)
{
    hal_video_present(FrameBuf, WIDTH, HEIGHT, WIDTH);
    return 1;
}

// ---------------------------------------------------------------------------
// Áudio (o EMULib gera PCM mono; o HAL leva até o DAC)
// ---------------------------------------------------------------------------
unsigned int InitAudio(unsigned int Rate, unsigned int Latency)
{
    (void)Rate; (void)Latency;
    return HAL_AUDIO_RATE;
}

void TrashAudio(void)
{
}

unsigned int GetFreeAudio(void)
{
    // WriteAudio bloqueia se a fila encher, então não devolva menos que um bloco curto.
    unsigned room = hal_audio_free();
    return room > 32 ? room : 32;
}

void PlayAllSound(int uSec)
{
    // Preserva a fração de amostra entre chamadas curtas (uma por grupo de scanlines).
    static uint64_t fraction;
    fraction += (uint64_t)uSec * HAL_AUDIO_RATE;
    unsigned int samples = (unsigned int)(fraction / 1000000u);
    fraction %= 1000000u;

    // O estado dos canais é atualizado pelo núcleo a cada bloco: sintetize agora.
    unsigned int played = RenderAndPlayAudio(samples);
    if (played < samples)
        fraction += (uint64_t)(samples - played) * 1000000u;
}

unsigned int WriteAudio(sample *Data, unsigned int Length)
{
    stat_where = "audio-write";     // se o emulador parar aqui, o áudio não está drenando
    hal_audio_write((const int16_t *)Data, Length);
    stat_where = "run";
    return Length;
}

// ---------------------------------------------------------------------------
// Partida: escolhe a pasta da BIOS, lê o /sd/bootl.rc e chama o fMSX
// ---------------------------------------------------------------------------
static int file_exists(const char *dir, const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

// Basta MSX2.ROM + MSX2EXT.ROM para ligar um MSX2.
static int bios_folder_ok(const char *dir)
{
    return file_exists(dir, "MSX2/MSX2.ROM") && file_exists(dir, "MSX2/MSX2EXT.ROM");
}

static char BiosDir[192];
static char RomArg[192], DiskAArg[192], DiskBArg[192];
static char SkipArg[8];

static void absolute_sd_path(char *dst, size_t n, const char *value)
{
    if (strncmp(value, "/sd/", 4) == 0 || strcmp(value, "/sd") == 0)
        snprintf(dst, n, "%s", value);
    else
        snprintf(dst, n, "/sd/%s", value[0] == '/' ? value + 1 : value);
}

static int select_bios_folder(void)
{
    static const char *candidates[] = {
        "/sd/msx", "/sd/MSX", "/sd/retro-go/bios/msx", "/sd/bios/msx", "/sd",
    };
    char value[160];
    if (hal_rc_get("msx_biosdir", value, sizeof(value)) && value[0]) {
        absolute_sd_path(BiosDir, sizeof(BiosDir), value);
        return bios_folder_ok(BiosDir);
    }
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (bios_folder_ok(candidates[i])) {
            snprintf(BiosDir, sizeof(BiosDir), "%s", candidates[i]);
            return 1;
        }
    }
    snprintf(BiosDir, sizeof(BiosDir), "/sd/msx");
    return 0;
}

void msx_run(void)
{
    char value[160];

    if (hal_rc_get("msx_frameskip", value, sizeof(value))) {
        int v = atoi(value);
        if (v >= 0 && v <= 75 && v % 25 == 0) FrameSkip = v;
    }
    if (hal_rc_get("msx_autoskip", value, sizeof(value)))
        AutoFrameSkip = atoi(value) != 0;
    if (hal_rc_get("msx_fm", value, sizeof(value)))
        SetFMEnabled(atoi(value) != 0);
    EffectiveFrameSkip = FrameSkip;
    snprintf(SkipArg, sizeof(SkipArg), "%d", FrameSkip);

    if (!select_bios_folder()) {
        hal_screen_message("BIOS do MSX2 nao encontrada",
                           "Coloque MSX2.ROM e MSX2EXT.ROM em /msx no cartao SD");
        hal_log("BIOS: MSX2/MSX2.ROM   MSX2/MSX2EXT.ROM ausentes (procurei em %s)\n", BiosDir);
        return;
    }
    hal_log("BIOS: %s\n", BiosDir);

    const char *argv[24];
    int argc = 0;
    argv[argc++] = "fmsx";
    argv[argc++] = "-msx2";
    argv[argc++] = "-ram";   argv[argc++] = "8";     // MSX2: 128 KB de RAM
    argv[argc++] = "-vram";  argv[argc++] = "8";     // MSX2: 128 KB de VRAM
    argv[argc++] = "-skip";  argv[argc++] = SkipArg;
    argv[argc++] = "-home";  argv[argc++] = BiosDir;
    argv[argc++] = "-joy";   argv[argc++] = "1";

    if (hal_rc_get("msx_diska", value, sizeof(value)) && value[0]) {
        absolute_sd_path(DiskAArg, sizeof(DiskAArg), value);
        argv[argc++] = "-diska"; argv[argc++] = DiskAArg;
    }
    if (hal_rc_get("msx_diskb", value, sizeof(value)) && value[0]) {
        absolute_sd_path(DiskBArg, sizeof(DiskBArg), value);
        argv[argc++] = "-diskb"; argv[argc++] = DiskBArg;
    }
    if (hal_rc_get("msx_rom", value, sizeof(value)) && value[0]) {
        absolute_sd_path(RomArg, sizeof(RomArg), value);
        argv[argc++] = RomArg;
    }
    argv[argc] = NULL;

    hal_log("fMSX start\n");
    stat_where = "start";
    int rc = fmsx_main(argc, (char **)argv);
    hal_log("fMSX ended (%d)\n", rc);
    if (rc) {
        hal_screen_message("fMSX terminou com erro", "Veja o monitor serial (115200)");
        return;
    }
    hal_restart();   // "Reboot" no menu do fMSX
}

// Diagnóstico (usado pela tarefa de FPS do main.cpp).
void msx_stats(unsigned *emu_frames, unsigned *drawn_frames_total, const char **where)
{
    *emu_frames = stat_emu_frames;
    *drawn_frames_total = stat_drawn_frames;
    *where = (const char *)stat_where;
}