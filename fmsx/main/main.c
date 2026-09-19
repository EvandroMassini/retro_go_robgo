#include <rg_system.h>
#include <string.h>
#ifdef RG_TARGET_ROBGO_RG
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
// Um produtor (emulacao) e um consumidor (I2S). ~80 ms de PCM mono.
#define PCM_BLOCK_SAMPLES 64
#define PCM_STREAM_SAMPLES 2560
#define PCM_STREAM_BYTES (PCM_STREAM_SAMPLES * sizeof(int16_t))
static StreamBufferHandle_t pcm_stream;
static int16_t pcm_pending[PCM_BLOCK_SAMPLES];
static unsigned pcm_pending_count;
static int64_t control_start;
static unsigned control_frames, recovery_windows;
#endif

#define AUDIO_SAMPLE_RATE (32000)
#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / 60 + 1)

static rg_surface_t *updates[2];
static rg_surface_t *currentUpdate;
static rg_task_t *audioQueue;
static rg_app_t *app;

static int JoyState, LastKey, InMenu, InKeyboard;
static int KeyboardCol, KeyboardRow, KeyboardKey;
static int64_t KeyboardDebounce = 0;
static int64_t FrameStartTime;
static int FrameSkip = 50;
static int EffectiveFrameSkip = 50;
static bool AutoFrameSkip = true;
static int KeyboardEmulation, CropPicture;
static char *PendingLoadSTA = NULL;

// Reinicie o controle de ritmo depois de menus/pausas; nao mede diagnosticos.
static void reset_frame_timing(void)
{
    FrameStartTime = rg_system_timer();
#ifdef RG_TARGET_ROBGO_RG
    control_start = 0;
    control_frames = recovery_windows = 0;
#endif
}

#define BPS16
#define BPP16
#define UNIX
#define GenericSetVideo SetVideo
#define LSB_FIRST
#define NARROW
#define WIDTH 256
#define HEIGHT 228
#define XKEYS 12
#define YKEYS 6

void PutImage(void);

static uint16_t BPal[256];
static uint16_t XPal[80];
static uint16_t XPal0;
static uint16_t *XBuf;

#include <fmsx.h>

static Image NormScreen;
const char *Title = "fMSX 6.0";
const char *Disks[2][MAXDISKS + 1];

static const unsigned char KBDKeys[YKEYS][XKEYS] = {
    {0x1B, CON_F1, CON_F2, CON_F3, CON_F4, CON_F5, CON_F6, CON_F7, CON_F8, CON_INSERT, CON_DELETE, CON_STOP},
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='},
    {CON_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', CON_BS},
    {'^', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', CON_ENTER},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0, 0},
    {'[', ']', ' ', ' ', ' ', ' ', ' ', '\\', '\'', 0, 0, 0}};

static const char *BiosFolder = RG_BASE_PATH_BIOS "/msx";
// We only check for absolutely essential files to avoid slowing down boot too much!
static const char *BiosFiles[] = {
    "MSX.ROM",
    "MSX2.ROM",
    "MSX2EXT.ROM",
    // "MSX2P.ROM",
    // "MSX2PEXT.ROM",
    // "FMPAC.ROM",
    "DISK.ROM",
    "MSXDOS2.ROM",
    // "PAINTER.ROM",
    // "KANJI.ROM",
};

static char bios_folder_buffer[RG_PATH_MAX + 1];

static bool bios_folder_complete(const char *folder)
{
    char path[RG_PATH_MAX + 1];
    for (size_t i = 0; i < RG_COUNT(BiosFiles); ++i) {
        int n = snprintf(path, sizeof(path), "%s/%s", folder, BiosFiles[i]);
        if (n < 0 || n >= sizeof(path) || !rg_storage_exists(path)) return false;
    }
    return true;
}

static void select_bios_folder(const char *rom)
{
    FILE *fp = fopen(RG_STORAGE_ROOT "/bootl.rc", "rb");
    if (fp) {
        char line[RG_PATH_MAX + 32];
        while (fgets(line, sizeof(line), fp)) {
            char *s = line;
            if (strncmp(s, "\xEF\xBB\xBF", 3) == 0) s += 3;
            while (*s == ' ' || *s == '\t') ++s;
            if (strncmp(s, "msx_biosdir", 11) != 0) continue;
            s += 11;
            while (*s == ' ' || *s == '\t') ++s;
            if (*s++ != '=') continue;
            while (*s == ' ' || *s == '\t') ++s;
            size_t len = strlen(s);
            while (len && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' ' || s[len-1] == '\t')) s[--len] = 0;
            if (!len) continue;
            int n;
            if (strncmp(s, RG_STORAGE_ROOT "/", strlen(RG_STORAGE_ROOT) + 1) == 0)
                n = snprintf(bios_folder_buffer, sizeof(bios_folder_buffer), "%s", s);
            else
                n = snprintf(bios_folder_buffer, sizeof(bios_folder_buffer), "%s/%s", RG_STORAGE_ROOT, *s == '/' ? s+1 : s);
            if (n < 0 || n >= sizeof(bios_folder_buffer)) {
                RG_LOGW("BIOS directory too long; ignoring msx_biosdir");
                continue;
            }
            BiosFolder = bios_folder_buffer;
            fclose(fp);
            RG_LOGI("BIOS directory from bootl.rc: %s", BiosFolder);
            return;
        }
        fclose(fp);
    }
    if (!bios_folder_complete(BiosFolder) && rom) {
        int n = snprintf(bios_folder_buffer, sizeof(bios_folder_buffer), "%s", rom);
        char *slash = strrchr(bios_folder_buffer, '/');
        if (n > 0 && n < sizeof(bios_folder_buffer) && slash) {
            *slash = 0;
            if (bios_folder_complete(bios_folder_buffer)) BiosFolder = bios_folder_buffer;
        }
    }
    RG_LOGI("BIOS directory selected: %s", BiosFolder);
}

static inline void PrepareFrame(void)
{
    int crop_v = CropPicture ? (ScanLines212 ? 8 : 18) : 0;
    currentUpdate->offset = crop_v * currentUpdate->stride;
    currentUpdate->height = HEIGHT - crop_v * 2;
}

static inline bool SubmitFramePrepared(bool block)
{
    PrepareFrame();
    bool queued = block ? (rg_display_submit(currentUpdate, 0), true)
                        : rg_display_try_submit(currentUpdate, 0);
    return queued;
}

static inline void SubmitFrame(void)
{
    SubmitFramePrepared(true);
}

#ifdef RG_TARGET_ROBGO_RG
#include "rg_robgo.h"
#endif

int ProcessEvents(int Wait)
{
    for (int i = 0; i < 16; ++i)
        KeyState[i] = 0xFF;
    JoyState = 0;
#ifdef RG_TARGET_ROBGO_RG
    uint8_t physical_keys[256];
    rg_robgo_keys(physical_keys);
    for (int k = 1; k < 130; ++k)
        if (physical_keys[k]) { KBD_SET(k); }
#endif

    uint32_t joystick = rg_input_read_gamepad();

    if (joystick == RG_KEY_MENU)
    {
        rg_gui_game_menu();
        reset_frame_timing();
        return 0;
    }
    else if (joystick == RG_KEY_OPTION)
    {
        rg_gui_options_menu();
        reset_frame_timing();
        return 0;
    }
    else if (joystick == RG_KEY_SELECT)
    {
        InKeyboard = !InKeyboard;
        rg_input_wait_for_key(RG_KEY_ANY, false, 500);
    }
    else if (joystick == RG_KEY_START)
    {
        // I think this key could be better used for something else
        // but for now the feedback is to keep a key for fMSX menu...
        InMenu = 2;
        return 0;
    }

    if (InMenu == 2)
    {
        InMenu = 1;
        rg_audio_set_mute(true);
        MenuMSX();
        rg_audio_set_mute(false);
        rg_input_wait_for_key(RG_KEY_ANY, false, 500);
        InMenu = 0;
        reset_frame_timing();
    }
    else if (InMenu)
    {
        if (joystick == RG_KEY_LEFT)
            LastKey = CON_LEFT;
        if (joystick == RG_KEY_RIGHT)
            LastKey = CON_RIGHT;
        if (joystick == RG_KEY_UP)
            LastKey = CON_UP;
        if (joystick == RG_KEY_DOWN)
            LastKey = CON_DOWN;
        if (joystick == RG_KEY_A)
            LastKey = CON_OK;
        if (joystick == RG_KEY_B)
            LastKey = CON_EXIT;
    }
    else if (InKeyboard)
    {
        if (joystick & (RG_KEY_LEFT | RG_KEY_RIGHT | RG_KEY_UP | RG_KEY_DOWN))
        {
            if (rg_system_timer() > KeyboardDebounce)
            {
                if (joystick == RG_KEY_LEFT)
                    KeyboardCol--;
                if (joystick == RG_KEY_RIGHT)
                    KeyboardCol++;
                if (joystick == RG_KEY_UP)
                    KeyboardRow--;
                if (joystick == RG_KEY_DOWN)
                    KeyboardRow++;

                KeyboardCol = RG_MIN(RG_MAX(KeyboardCol, 0), XKEYS - 1);
                KeyboardRow = RG_MIN(RG_MAX(KeyboardRow, 0), YKEYS - 1);
                PutImage();
                KeyboardDebounce = rg_system_timer() + 250000;
            }
        }
        else if (joystick == RG_KEY_A)
        {
            KeyboardKey = KBDKeys[KeyboardRow][KeyboardCol];
            KBD_SET(KeyboardKey);
        }
        else if (joystick == RG_KEY_B)
        {
            rg_input_wait_for_key(RG_KEY_ANY, false, 500);
            InKeyboard = false;
        }
    }
#ifndef RG_TARGET_ROBGO_RG
    else if (KeyboardEmulation)
    {
        if (joystick & RG_KEY_LEFT)
            KBD_SET(KBD_LEFT);
        if (joystick & RG_KEY_RIGHT)
            KBD_SET(KBD_RIGHT);
        if (joystick & RG_KEY_UP)
            KBD_SET(KBD_UP);
        if (joystick & RG_KEY_DOWN)
            KBD_SET(KBD_DOWN);
        if (joystick & RG_KEY_A)
            KBD_SET(KBD_SPACE);
        if (joystick & RG_KEY_B)
            KBD_SET(KBD_ENTER);
    }
#endif
    else
    {
        if (joystick & RG_KEY_LEFT)
            JoyState |= JST_LEFT;
        if (joystick & RG_KEY_RIGHT)
            JoyState |= JST_RIGHT;
        if (joystick & RG_KEY_UP)
            JoyState |= JST_UP;
        if (joystick & RG_KEY_DOWN)
            JoyState |= JST_DOWN;
        if (joystick & RG_KEY_A)
            JoyState |= JST_FIREA;
        if (joystick & RG_KEY_B)
            JoyState |= JST_FIREB;
    }

    return 0;
}

int InitMachine(void)
{
    NormScreen = (Image){
        .Data = currentUpdate->data,
        .W = WIDTH,
        .H = HEIGHT,
        .L = WIDTH,
        .D = 16,
    };

    XBuf = NormScreen.Data;
    SetScreenDepth(NormScreen.D);
    SetVideo(&NormScreen, 0, 0, WIDTH, HEIGHT);

    for (int J = 0; J < 80; J++)
        SetColor(J, 0, 0, 0);

    for (int J = 0; J < 256; J++)
    {
        uint16_t color = C_RGB(((J >> 2) & 0x07) * 255 / 7, ((J >> 5) & 0x07) * 255 / 7, (J & 0x03) * 255 / 3);
        BPal[J] = ((color >> 8) | (color << 8)) & 0xFFFF;
    }

    InitSound(AUDIO_SAMPLE_RATE, 150);
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
    uint16_t color = C_RGB(R, G, B);
    color = (color >> 8) | (color << 8);
    if (N)
        XPal[N] = color;
    else
        XPal0 = color;
}

void PutImage(void)
{
    if (InKeyboard)
        DrawKeyboard(&NormScreen, KBDKeys[KeyboardRow][KeyboardCol]);

#ifdef RG_TARGET_ROBGO_RG
    // Nao bloqueie a emulacao no blit: sem PCM novo o I2S picota.
    if (!SubmitFramePrepared(false))
        return;
#else
    SubmitFrame();
#endif
    currentUpdate = updates[currentUpdate == updates[0]];
    NormScreen.Data = currentUpdate->data;
    XBuf = NormScreen.Data;
}

unsigned int Joystick(void)
{
    ProcessEvents(0);
    return JoyState;
}

void Keyboard(void)
{
#ifdef RG_TARGET_ROBGO_RG
    // O MSX consulta a matriz diretamente: atualize mesmo sem leitura de joystick.
    // Isso inclui jogos pausados que aguardam F1 para continuar.
    ProcessEvents(0);
#endif
    int target = PALVideo ? 50 : 60;
    if (rg_system_get_tick_rate() != target) rg_system_set_tick_rate(target);
    int64_t now = rg_system_timer();
#ifdef RG_TARGET_ROBGO_RG
    // Controle lento com histerese: sacrifique desenho, nunca ciclos MSX.
    if (!control_start) control_start = now;
    ++control_frames;
    if (now - control_start >= 1000000) {
        double rate = control_frames * 1000000.0 / (now - control_start);
        size_t queued = xStreamBufferBytesAvailable(pcm_stream);
        if (AutoFrameSkip && now - control_start < 2000000) {
            if (rate < target * 0.97 && queued < PCM_STREAM_BYTES / 8) {
                EffectiveFrameSkip = RG_MIN(75, EffectiveFrameSkip + 5);
                recovery_windows = 0;
            } else if (rate >= target * 0.98 && queued >= PCM_STREAM_BYTES / 2) {
                if (++recovery_windows >= 10) {
                    EffectiveFrameSkip = RG_MAX(FrameSkip, EffectiveFrameSkip - 5);
                    recovery_windows = 0;
                }
            } else recovery_windows = 0;
        } else {
            recovery_windows = 0;
            if (!AutoFrameSkip) EffectiveFrameSkip = FrameSkip;
        }
        UPeriod = 100 - EffectiveFrameSkip;
        control_start = now;
        control_frames = 0;
    }
#endif
    // Keyboard() is a convenient place to do our vsync stuff :)
    rg_system_tick(rg_system_timer() - FrameStartTime);
    FrameStartTime = rg_system_timer();

    if (PendingLoadSTA)
    {
        LoadSTA(PendingLoadSTA);
        free(PendingLoadSTA);
        PendingLoadSTA = NULL;
    }
}

unsigned int Mouse(byte N)
{
#ifdef RG_TARGET_ROBGO_RG
    return N == 0 ? rg_robgo_mouse() : 0;
#endif
    return 0;
}

int ShowVideo(void)
{
    SubmitFrame();
    rg_system_tick(0);
    return 1;
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
    GetKey();
    rg_input_wait_for_key(RG_KEY_ANY, false, 200);
    while (!rg_input_wait_for_key(RG_KEY_ANY, true, 100))
        continue;
    return GetKey();
}

unsigned int WaitKeyOrMouse(void)
{
    LastKey = WaitKey();
    return 0;
}

unsigned int InitAudio(unsigned int Rate, unsigned int Latency)
{
    return AUDIO_SAMPLE_RATE;
}

void TrashAudio(void)
{
    //
}

unsigned int GetFreeAudio(void)
{
#ifdef RG_TARGET_ROBGO_RG
    size_t stream_samples = xStreamBufferSpacesAvailable(pcm_stream) / sizeof(int16_t);
    unsigned room = (unsigned)stream_samples + (PCM_BLOCK_SAMPLES - pcm_pending_count);
    // Nao corte blocos curtos: WriteAudio bloqueia se a fila encher.
    return room > 32 ? room : 32;
#else
    return 1024;
#endif
}

void PlayAllSound(int uSec)
{
    // EMULib sintetiza mono. Preserve a fracao entre blocos curtos de scanlines.
    static unsigned int sample_fraction;
    sample_fraction += uSec * AUDIO_SAMPLE_RATE;
    unsigned int samples = sample_fraction / 1000000;
    sample_fraction %= 1000000;
#ifdef RG_TARGET_ROBGO_RG
    // O estado WaveCH e atualizado pelo nucleo MSX. Sintetize aqui, antes
    // que o proximo bloco altere frequencia/volume; transporte somente PCM.
    unsigned int played = RenderAndPlayAudio(samples);
    if (played < samples)
        sample_fraction += (unsigned int)(samples - played) * 1000000u;
#else
    int64_t start = rg_system_timer();
    rg_task_send(audioQueue, &(rg_task_msg_t){.dataInt = samples});
    int64_t waited = rg_system_timer() - start;
    FrameStartTime += waited;
#endif
}

unsigned int WriteAudio(sample *Data, unsigned int Length)
{
#ifdef RG_TARGET_ROBGO_RG
    for (unsigned i = 0; i < Length; ++i) {
        pcm_pending[pcm_pending_count++] = Data[i];
        if (pcm_pending_count == PCM_BLOCK_SAMPLES) {
            int64_t start = rg_system_timer();
            size_t sent = xStreamBufferSend(pcm_stream, pcm_pending, sizeof(pcm_pending), portMAX_DELAY);
            RG_ASSERT(sent == sizeof(pcm_pending), "PCM stream short write");
            int64_t waited = rg_system_timer() - start;
            FrameStartTime += waited;
            pcm_pending_count = 0;
        }
    }
#else
    // Cada amostra mono vira um frame estereo, sem sintetizar duas vezes
    // nem tratar amostras consecutivas como canais esquerdo/direito.
    rg_audio_frame_t frames[64];
    for (unsigned int pos = 0; pos < Length;) {
        unsigned int count = RG_MIN(Length - pos, RG_COUNT(frames));
        for (unsigned int i = 0; i < count; ++i)
            frames[i] = (rg_audio_frame_t){Data[pos + i], Data[pos + i]};
        rg_audio_submit(frames, count);
        pos += count;
    }
#endif
    return Length;
}

static bool save_state_handler(const char *filename)
{
    return SaveSTA(filename);
}

static bool load_state_handler(const char *filename)
{
    PendingLoadSTA = strdup(filename);
    return true;
}

static bool reset_handler(bool hard)
{
    ResetMSX(Mode,RAMPages,VRAMPages);
    return true;
}

static bool screenshot_handler(const char *filename, int width, int height)
{
    return rg_surface_save_image_file(currentUpdate, filename, width, height);
}

static void event_handler(int event, void *arg)
{
    if (event == RG_EVENT_REDRAW)
    {
        SubmitFrame();
    }
}

static rg_gui_event_t crop_select_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT)
    {
        CropPicture = !CropPicture;
        rg_settings_set_number(NS_APP, "Crop", CropPicture);
        return RG_DIALOG_REDRAW;
    }
    strcpy(option->value, CropPicture ? _("On") : _("Off"));
    return RG_DIALOG_VOID;
}

static rg_gui_event_t input_select_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT)
    {
        KeyboardEmulation = !KeyboardEmulation;
        rg_settings_set_number(NS_APP, "Input", KeyboardEmulation);
    }
    strcpy(option->value, KeyboardEmulation ? _("Keyboard") : _("Joystick"));
    return RG_DIALOG_VOID;
}

static void audioTask(void *arg)
{
#ifdef RG_TARGET_ROBGO_RG
    RG_LOGI("PCM consumer core=%d priority=%u block=%d samples; producer on emulation core",
            xPortGetCoreID(), (unsigned)uxTaskPriorityGet(NULL), PCM_BLOCK_SAMPLES);
    int16_t mono[PCM_BLOCK_SAMPLES];
    rg_audio_frame_t frames[PCM_BLOCK_SAMPLES];
    rg_audio_frame_t last_frame = {0, 0};
    bool recovering = false;
    for (;;) {
        size_t bytes = xStreamBufferReceive(pcm_stream, mono, sizeof(mono), 0);
        size_t count = bytes / sizeof(int16_t);
        if (!bytes) {
            // O DMA dita o ritmo: nao espere 20 ms para entregar apenas 2 ms.
            // Complete um bloco com fade ate zero; faltas seguintes sao silencio.
            for (unsigned i = 0; i < PCM_BLOCK_SAMPLES; ++i) {
                int gain = PCM_BLOCK_SAMPLES - 1 - i;
                frames[i] = (rg_audio_frame_t){
                    last_frame.left * gain / PCM_BLOCK_SAMPLES,
                    last_frame.right * gain / PCM_BLOCK_SAMPLES};
            }
            last_frame = (rg_audio_frame_t){0, 0};
            recovering = true;
            rg_audio_submit(frames, PCM_BLOCK_SAMPLES);
            continue;
        }
        RG_ASSERT(!(bytes & 1), "PCM stream alignment");
        for (size_t i = 0; i < count; ++i) {
            int sample = mono[i];
            if (recovering) {
                // Retome sem salto abrupto, depois de falta temporaria de PCM.
                frames[i] = (rg_audio_frame_t){
                    (last_frame.left * (int)(count-1-i) + sample * (int)(i+1)) / (int)count,
                    (last_frame.right * (int)(count-1-i) + sample * (int)(i+1)) / (int)count};
            } else frames[i] = (rg_audio_frame_t){sample, sample};
        }
        recovering = false;
        last_frame = frames[count - 1];
        rg_audio_submit(frames, count);
    }
#else
    RG_LOGI("task started");
    rg_task_msg_t msg;
    while (rg_task_peek(&msg))
    {
        RenderAndPlayAudio(msg.dataInt);
        rg_task_receive(&msg);
    }
#endif
}

static rg_gui_event_t frameskip_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) {
        FrameSkip = (FrameSkip + (event == RG_DIALOG_NEXT ? 25 : 75)) % 100;
        EffectiveFrameSkip = FrameSkip;
        UPeriod = 100 - FrameSkip;
        rg_settings_set_number(NS_APP, "FrameSkip", FrameSkip);
    }
    sprintf(option->value, "%d%%", FrameSkip);
    return RG_DIALOG_VOID;
}

static rg_gui_event_t auto_frameskip_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) {
        AutoFrameSkip = !AutoFrameSkip;
        EffectiveFrameSkip = FrameSkip;
        UPeriod = 100 - FrameSkip;
        rg_settings_set_number(NS_APP, "AutoFrameSkip", AutoFrameSkip);
    }
    strcpy(option->value, AutoFrameSkip ? "On" : "Off");
    return RG_DIALOG_VOID;
}

#ifdef RG_TARGET_ROBGO_RG
static rg_gui_event_t fm_synth_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) {
        SetFMEnabled(!FMEnabled);
        rg_settings_set_number(NS_APP, "FMSynth", FMEnabled);
        RG_LOGI("FM synth=%s; PSG/SCC preserved", FMEnabled ? "On" : "Off");
    }
    strcpy(option->value, FMEnabled ? "On" : "Off");
    return RG_DIALOG_VOID;
}
#endif

static void options_handler(rg_gui_option_t *dest)
{
    *dest++ = (rg_gui_option_t){0, _("Input"), "-", RG_DIALOG_FLAG_NORMAL, &input_select_cb};
    *dest++ = (rg_gui_option_t){0, _("Crop"),  "-", RG_DIALOG_FLAG_NORMAL, &crop_select_cb};
    *dest++ = (rg_gui_option_t){0, "Frame skip", "-", RG_DIALOG_FLAG_NORMAL, &frameskip_cb};
#ifdef RG_TARGET_ROBGO_RG
    *dest++ = (rg_gui_option_t){0, "Auto frame skip", "-", RG_DIALOG_FLAG_NORMAL, &auto_frameskip_cb};
    *dest++ = (rg_gui_option_t){0, "FM synth", "-", RG_DIALOG_FLAG_NORMAL, &fm_synth_cb};
#endif
    *dest++ = (rg_gui_option_t)RG_DIALOG_END;
}

void app_main(void)
{
    const rg_handlers_t handlers = {
        .loadState = &load_state_handler,
        .saveState = &save_state_handler,
        .reset = &reset_handler,
        .screenshot = &screenshot_handler,
        .event = &event_handler,
        .options = &options_handler,
    };

    app = rg_system_init(AUDIO_SAMPLE_RATE, &handlers, NULL);
    rg_system_set_tick_rate(60);
#ifdef RG_TARGET_ROBGO_RG
    FMEnabled = rg_settings_get_number(NS_APP, "FMSynth", 1) != 0;
    RG_LOGI("Mixer: PSG 0..5 SCC 6..10 FM 11..19; FM synth=%s", FMEnabled ? "On" : "Off");
#endif
    RG_LOGI("MSX tasks: emulation core 0, display/audio core 1; VGA scanout via DMA");

    updates[0] = rg_surface_create(WIDTH, HEIGHT, RG_PIXEL_565_BE, MEM_FAST);
    updates[1] = rg_surface_create(WIDTH, HEIGHT, RG_PIXEL_565_BE, MEM_FAST);
    currentUpdate = updates[0];

    KeyboardEmulation = rg_settings_get_number(NS_APP, "Input", 1);
    CropPicture = rg_settings_get_number(NS_APP, "Crop", 0);
    FrameSkip = rg_settings_get_number(NS_APP, "FrameSkip", 50);
    if (FrameSkip < 0 || FrameSkip > 75 || FrameSkip % 25) FrameSkip = 50;
    EffectiveFrameSkip = FrameSkip;
    AutoFrameSkip = rg_settings_get_number(NS_APP, "AutoFrameSkip", 1) != 0;
#ifdef RG_TARGET_ROBGO_RG
    RG_LOGI("Active settings: ROM=%s FM=%s skip=%d auto=%d output=%s rate=%d speed=%.2f",
        app->romPath, FMEnabled ? "On" : "Off", FrameSkip, AutoFrameSkip,
        rg_audio_get_sink()->name, rg_audio_get_sample_rate(), rg_system_get_app_speed());
#endif
    char skip_arg[4];
    snprintf(skip_arg, sizeof(skip_arg), "%d", FrameSkip);

    select_bios_folder(app->romPath);
    for (size_t i = 0; i < RG_COUNT(BiosFiles); ++i)
    {
        char pathbuf[RG_PATH_MAX + 1];
        snprintf(pathbuf, RG_PATH_MAX, "%s/%s", BiosFolder, BiosFiles[i]);
        if (!rg_storage_exists(pathbuf))
        {
            char message[512];
            snprintf(message, 512, "File: %s\nYou can find it at:\n%s",
                        rg_relpath(pathbuf), "https://fms.komkon.org/fMSX/");
            rg_gui_alert(_("BIOS file missing!"), message);
        }
    }

    if (app->bootFlags & RG_BOOT_RESUME)
    {
        PendingLoadSTA = rg_emu_get_path(RG_PATH_SAVE_STATE + app->saveSlot, app->romPath);
    }

    const char *argv[] = {
        "fmsx",
        "-ram", "2",
        "-vram", "2",
        "-skip", skip_arg,
        "-home", BiosFolder,
        "-joy", "1",
        NULL, NULL, NULL,
    };
    int argc = RG_COUNT(argv) - 3;

    if (rg_extension_match(app->romPath, "dsk"))
    {
        argv[argc++] = "-diska";
    }
    argv[argc++] = app->romPath;

#ifdef RG_TARGET_ROBGO_RG
    pcm_stream = xStreamBufferCreate(PCM_STREAM_BYTES, PCM_BLOCK_SAMPLES * sizeof(int16_t));
    RG_ASSERT(pcm_stream, "Cannot allocate PCM stream");
    audioQueue = rg_task_create("audioTask", &audioTask, NULL, 4096, RG_TASK_PRIORITY_7, 1);
    RG_ASSERT(audioQueue, "Cannot start PCM consumer");
    RG_LOGI("PCM producer core=%d; stream %u bytes; output core 1 priority 7; DMA configuration logged by driver",
            xPortGetCoreID(), (unsigned)PCM_STREAM_BYTES);
#else
    audioQueue = rg_task_create("audioTask", &audioTask, NULL, 4096, RG_TASK_PRIORITY_2, 1);
#endif

    RG_LOGI("fMSX start");
    fmsx_main(argc, (char **)argv);

    RG_LOGI("fMSX ended");
    rg_system_exit();
}
