/*
 * In-game voice chat for C1 (feat/voicechat). Included by kernel33.c after console.c, so it shares
 * Log/Readable/WriteCode/CallsTarget/RedirectCall and the console's ConPrintf/ConCbufAddText.
 *
 * C1 stripped IW's voice: no capture (no DirectSound/waveIn), no codec (the Speex tables are gone),
 * no CL_WriteVoicePacket, no +talk. What is left: the "v" branch of CL_ConnectionlessPacket (it only
 * prints "voice communication is disabled"), the voice dvars, two Scaleform callbacks that return
 * at once, and the head-icon talk balloon. This file puts the rest back:
 *
 *   capture  WASAPI shared mode, default capture endpoint, on a worker thread; converted to 48 kHz
 *            mono and encoded with libopus (VOIP, 20 ms frames, FEC, DTX), linked statically.
 *            The stream only runs while push-to-talk is held, so the mic is idle otherwise.
 *   send     main thread, once per Com_EventLoop pass (call 0x595E9D -> 0x595CF0 hooked): queued
 *            frames go to clc.serverAddress as OOB "v" via C1's NET_OutOfBandData 0x5A3D60
 *            (sock 0 = NS_CLIENT1), i.e. through the same bdDTLS association as the game.
 *            clc = [0x16F8068] ^ 0x26F950CC (CL_ConnectionlessPacket 0x496729), serverAddress at
 *            clc+0x14 (24 bytes); connection state [0x16FC2E8] == 9 (CA_ACTIVE).
 *   receive  CL_ConnectionlessPacket 0x4963E0: the "v" branch at 0x496482 (push "Received voice
 *            packet, but voice communication is disabled") jumps to VOICE_RecvHook, which hands
 *            msg ([ebp-0xC6C], msg_t: data +8, cursize +0x14) and from ([ebp+0x10], netadr_t) to
 *            VoiceOnPacket, then returns 1 the way the original branch does (0x496491).
 *   play     per-talker Opus decoder and jitter buffer (40 ms prebuffer, FEC from the next packet,
 *            PLC on loss), mixed and played through WASAPI on the same worker thread.
 *   keys     commands +talk/-talk/toggletalk (bindable: "bind CAPSLOCK +talk") linked into C1's
 *            cmd_function_t list 0x24F6424 {next, name, function}, plus a direct key (dvar
 *            k33_talkKey, Windows virtual key, default 20 = Caps Lock) read in the WndProc hook.
 *   ui       AS_EnableVoiceTalk (vtable 0xC77100) and AS_SetVoiceMicMute (0xC636A8) Call slots,
 *            both `ret 4` at 0x97C8B0, go to VoiceAsEnable / VoiceAsMicMute.
 *
 * Wire format (see CoD4x src/codol/net/codol_voice.c, the relay):
 *   client -> server  "\xff\xff\xff\xffv\0" u8 ver=1, u8 flags (1 = last), u16 seq, u8 n, n*(u8 len, data)
 *   server -> client  "\xff\xff\xff\xffv\0" u8 ver=1, u8 flags, u8 talker, u16 seq, u8 n, frames
 *
 * Dvars: codol_voice (master switch; "+set codol_voice 0" installs nothing), k33_talkKey,
 * k33_voiceVolume (percent), k33_voiceBitrate, k33_voiceLoopback (hear yourself locally, no server),
 * k33_voiceDebug. Commands: +talk, -talk, toggletalk, k33voice (status), k33voicemute <client>.
 *
 * Every site is byte-checked; a mismatch is logged and that part stays off. Missing audio devices
 * only disable capture or playback (retried every few seconds), never the game.
 */

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <opus/opus.h>
#include <math.h>

#define VOICE_EVENTLOOP_CALL_VA 0x00595E9Du
#define VOICE_EVENTLOOP_FUNC_VA 0x00595CF0u
#define VOICE_RECV_VA           0x00496482u
#define VOICE_RECV_DONE_VA      0x00496491u
#define VOICE_OOBDATA_VA        0x005A3D60u
#define VOICE_CLC_PTR_VA        0x016F8068u
#define VOICE_CLC_KEY           0x26F950CCu
#define VOICE_CONNSTATE_VA      0x016FC2E8u
#define VOICE_CA_ACTIVE         9
#define VOICE_CMD_ARGS_VA       0x024F63A0u   /* nesting index; argc [+0x44 + i*4], argv [+0x64 + i*4] */
#define VOICE_AS_ENABLE_SLOT    0x00C77104u
#define VOICE_AS_MICMUTE_SLOT   0x00C636ACu
#define VOICE_AS_NOP            0x0097C8B0u

#define VOICE_RATE      48000
#define VOICE_FRAME     960      /* 20 ms */
#define VOICE_MAXFRAME  255
#define VOICE_TALKERS   64
#define VOICE_LOOPBACK_TALKER 63
#define VOICE_JITTER    32
#define VOICE_SENDQ     64
#define VOICE_PER_PACKET 3

typedef struct { int have; unsigned short seq; BYTE len; BYTE data[VOICE_MAXFRAME]; } VoiceSlot;

typedef struct {
    OpusDecoder *dec;
    VoiceSlot slot[VOICE_JITTER];
    int queued, started, lost, ending;
    unsigned short next;
    DWORD firstArrive, lastArrive;
    volatile LONG muted;
} VoiceTalker;

typedef struct { unsigned short seq; BYTE len, last; BYTE data[VOICE_MAXFRAME]; DWORD made; } VoiceOut;

static BOOL g_voiceOn;                         /* installed */
static BOOL g_voiceSendOk, g_voiceRecvOk, g_voiceCmdsOk, g_voiceKeyOk;
static CRITICAL_SECTION g_voiceLock;           /* talkers + send queue */
static VoiceTalker g_talkers[VOICE_TALKERS];
static VoiceOut g_sendq[VOICE_SENDQ];
static int g_sendHead, g_sendCount;
static volatile LONG g_keyTalk, g_cmdTalk, g_toggleTalk, g_micMuted, g_uiVoiceOff;
static volatile LONG g_wantCapture;            /* main thread -> audio thread */
static volatile LONG g_connected;              /* CA_ACTIVE with a server address */
static volatile LONG g_statCapFrames, g_statSent, g_statRecv, g_statPlayed, g_statLostFrames;
static volatile LONG g_captureState, g_renderState; /* 0 not tried, 1 ok, -1 failed */
static volatile LONG g_audioStarted;
static char g_captureName[128], g_renderName[128];
static const BYTE *volatile g_dvVoice, *volatile g_dvKey, *volatile g_dvVolume, *volatile g_dvBitrate,
    *volatile g_dvLoopback, *volatile g_dvDebug, *volatile g_dvTone;
static BOOL g_voiceDvarsDone, g_voiceAnnounced;
static unsigned short g_sendSeq;
static volatile DWORD g_lastHeard[VOICE_TALKERS];
static int g_lastTalkPrint;

static void __cdecl VoiceCmdStatus(void);
static int VoiceDvarInt(const BYTE *d, int def) { return d ? *(volatile const int *)(d + 0xC) : def; }

/* ---- jitter buffer (any thread, under g_voiceLock) ------------------------------------------ */

static void VoiceTalkerReset(VoiceTalker *t)
{
    for (int i = 0; i < VOICE_JITTER; i++) t->slot[i].have = 0;
    t->queued = t->started = t->lost = t->ending = 0;
}

static void VoiceInsert(int talker, unsigned short seq, const BYTE *data, int len, BOOL last)
{
    if (talker < 0 || talker >= VOICE_TALKERS || len < 1 || len > VOICE_MAXFRAME) return;
    VoiceTalker *t = &g_talkers[talker];
    DWORD now = GetTickCount();
    if (!t->started && !t->queued) { t->next = seq; t->firstArrive = now; t->ending = 0; }
    short ahead = (short)(seq - t->next);
    if (ahead < 0) return; /* late: already played or concealed */
    if (ahead >= VOICE_JITTER) { VoiceTalkerReset(t); t->next = seq; t->firstArrive = now; ahead = 0; }
    VoiceSlot *s = &t->slot[seq % VOICE_JITTER];
    if (s->have && s->seq == seq) return; /* duplicate */
    if (!s->have) t->queued++;
    s->have = 1;
    s->seq = seq;
    s->len = (BYTE)len;
    memcpy(s->data, data, len);
    t->lastArrive = now;
    if (last) t->ending = 1;
    g_lastHeard[talker] = now;
}

/* One 20 ms frame of this talker into out (adds); 0 when silent. Audio thread. */
static int VoicePull(VoiceTalker *t, float *out, float *tmp)
{
    DWORD now = GetTickCount();
    if (!t->started) {
        if (!t->queued) return 0;
        if (t->queued < 2 && !t->ending && now - t->firstArrive < 60) return 0;
        t->started = 1;
        t->lost = 0;
    }
    if (!t->dec) {
        int err = 0;
        t->dec = opus_decoder_create(VOICE_RATE, 1, &err);
        if (!t->dec) { VoiceTalkerReset(t); return 0; }
    }
    VoiceSlot *s = &t->slot[t->next % VOICE_JITTER];
    VoiceSlot *n = &t->slot[(unsigned short)(t->next + 1) % VOICE_JITTER];
    int got;
    if (s->have && s->seq == t->next) {
        got = opus_decode_float(t->dec, s->data, s->len, tmp, VOICE_FRAME, 0);
        s->have = 0;
        t->queued--;
        t->lost = 0;
    } else if (!t->queued && (t->ending || t->lost >= 3)) {
        VoiceTalkerReset(t);
        return 0;
    } else {
        InterlockedIncrement(&g_statLostFrames);
        if (n->have && n->seq == (unsigned short)(t->next + 1))
            got = opus_decode_float(t->dec, n->data, n->len, tmp, VOICE_FRAME, 1); /* FEC */
        else
            got = opus_decode_float(t->dec, NULL, 0, tmp, VOICE_FRAME, 0);        /* PLC */
        if (++t->lost > 12) { VoiceTalkerReset(t); return 0; }
    }
    t->next++;
    if (got <= 0) return 0;
    if (t->muted) return 0;
    for (int i = 0; i < got && i < VOICE_FRAME; i++) out[i] += tmp[i];
    return 1;
}

/* ---- WASAPI (audio thread) ------------------------------------------------------------------ */

static const GUID kClsidEnum = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const GUID kIidEnum = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const GUID kIidClient = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const GUID kIidCapture = { 0xC8ADBD64, 0xE71E, 0x48A0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
static const GUID kIidRender = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
static const GUID kSubFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
static const GUID kSubPcm = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

typedef struct {
    IAudioClient *client;
    IAudioCaptureClient *cap;
    IAudioRenderClient *ren;
    HANDLE event;
    int channels, rate, bits, isFloat, blockAlign;
    UINT32 bufferFrames;
    BOOL running;
    DWORD retryAt;
    double pos;      /* resampler phase */
    float prev;      /* resampler: previous input sample */
} VoiceDev;

static VoiceDev g_cap, g_ren;

static void VoiceDevRelease(VoiceDev *d)
{
    if (d->client && d->running) IAudioClient_Stop(d->client);
    if (d->cap) IAudioCaptureClient_Release(d->cap);
    if (d->ren) IAudioRenderClient_Release(d->ren);
    if (d->client) IAudioClient_Release(d->client);
    if (d->event) CloseHandle(d->event);
    DWORD retry = d->retryAt;
    memset(d, 0, sizeof(*d));
    d->retryAt = retry;
}

static BOOL VoiceDevOpen(VoiceDev *d, BOOL capture)
{
    IMMDeviceEnumerator *en = NULL;
    IMMDevice *dev = NULL;
    WAVEFORMATEX *mix = NULL;
    HRESULT hr;
    const char *what = capture ? "capture" : "playback";
    memset(d, 0, sizeof(*d));
    hr = CoCreateInstance(&kClsidEnum, NULL, CLSCTX_ALL, &kIidEnum, (void **)&en);
    if (FAILED(hr)) { Log("voice: %s: MMDeviceEnumerator failed 0x%08lX", what, (unsigned long)hr); goto fail; }
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, capture ? eCapture : eRender, eCommunications, &dev);
    if (FAILED(hr)) hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, capture ? eCapture : eRender, eConsole, &dev);
    if (FAILED(hr)) { Log("voice: %s: no default device (0x%08lX)", what, (unsigned long)hr); goto fail; }
    hr = IMMDevice_Activate(dev, &kIidClient, CLSCTX_ALL, NULL, (void **)&d->client);
    if (FAILED(hr)) { Log("voice: %s: Activate failed 0x%08lX", what, (unsigned long)hr); goto fail; }
    hr = IAudioClient_GetMixFormat(d->client, &mix);
    if (FAILED(hr) || !mix) { Log("voice: %s: GetMixFormat failed 0x%08lX", what, (unsigned long)hr); goto fail; }
    d->channels = mix->nChannels;
    d->rate = (int)mix->nSamplesPerSec;
    d->bits = mix->wBitsPerSample;
    d->blockAlign = mix->nBlockAlign;
    d->isFloat = mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                 (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                  IsEqualGUID(&((WAVEFORMATEXTENSIBLE *)mix)->SubFormat, &kSubFloat));
    BOOL pcm = mix->wFormatTag == WAVE_FORMAT_PCM ||
               (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                IsEqualGUID(&((WAVEFORMATEXTENSIBLE *)mix)->SubFormat, &kSubPcm));
    if (!(d->isFloat && d->bits == 32) && !(pcm && (d->bits == 16 || d->bits == 32 || d->bits == 24))) {
        Log("voice: %s: unsupported mix format (tag %u, %u bits)", what, mix->wFormatTag, d->bits);
        goto fail;
    }
    if (capture) d->event = CreateEventA(NULL, FALSE, FALSE, NULL);
    hr = IAudioClient_Initialize(d->client, AUDCLNT_SHAREMODE_SHARED,
                                 capture ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0,
                                 capture ? 400000 : 1200000 /* 40 / 120 ms */, 0, mix, NULL);
    if (FAILED(hr)) { Log("voice: %s: Initialize failed 0x%08lX", what, (unsigned long)hr); goto fail; }
    if (capture && FAILED(IAudioClient_SetEventHandle(d->client, d->event))) goto fail;
    IAudioClient_GetBufferSize(d->client, &d->bufferFrames);
    hr = capture ? IAudioClient_GetService(d->client, &kIidCapture, (void **)&d->cap)
                 : IAudioClient_GetService(d->client, &kIidRender, (void **)&d->ren);
    if (FAILED(hr)) { Log("voice: %s: GetService failed 0x%08lX", what, (unsigned long)hr); goto fail; }
    _snprintf(capture ? g_captureName : g_renderName, 128, "%d Hz, %d ch, %d-bit %s", d->rate, d->channels,
              d->bits, d->isFloat ? "float" : "pcm");
    Log("voice: %s device open: %s, buffer %u frames", what, capture ? g_captureName : g_renderName,
        (unsigned)d->bufferFrames);
    CoTaskMemFree(mix);
    IMMDevice_Release(dev);
    IMMDeviceEnumerator_Release(en);
    return TRUE;
fail:
    if (mix) CoTaskMemFree(mix);
    if (dev) IMMDevice_Release(dev);
    if (en) IMMDeviceEnumerator_Release(en);
    VoiceDevRelease(d);
    d->retryAt = GetTickCount() + 5000;
    return FALSE;
}

static float VoiceSampleIn(const VoiceDev *d, const BYTE *frame)
{
    float sum = 0;
    for (int c = 0; c < d->channels; c++) {
        const BYTE *p = frame + c * (d->bits / 8);
        float v;
        if (d->isFloat) v = *(const float *)p;
        else if (d->bits == 16) v = *(const short *)p / 32768.0f;
        else if (d->bits == 24) v = (float)((int)((p[0] << 8) | (p[1] << 16) | (p[2] << 24)) >> 8) / 8388608.0f;
        else v = *(const int *)p / 2147483648.0f;
        sum += v;
    }
    return sum / d->channels;
}

static void VoiceSampleOut(const VoiceDev *d, BYTE *frame, float v)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    for (int c = 0; c < d->channels; c++) {
        BYTE *p = frame + c * (d->bits / 8);
        if (d->isFloat) *(float *)p = v;
        else if (d->bits == 16) *(short *)p = (short)(v * 32767.0f);
        else if (d->bits == 24) { int s = (int)(v * 8388607.0f); p[0] = (BYTE)s; p[1] = (BYTE)(s >> 8); p[2] = (BYTE)(s >> 16); }
        else *(int *)p = (int)(v * 2147483647.0);
    }
}

/* ---- capture + encode ----------------------------------------------------------------------- */

static OpusEncoder *g_enc;
static int g_encBitrate;
static float g_capFrame[VOICE_FRAME];
static int g_capFill;

static void VoiceQueueFrame(const BYTE *data, int len, BOOL last)
{
    if (len < 1 || len > VOICE_MAXFRAME) return;
    unsigned short seq = g_sendSeq++;
    if (g_dvLoopback && VoiceDvarInt(g_dvLoopback, 0)) {
        EnterCriticalSection(&g_voiceLock);
        VoiceInsert(VOICE_LOOPBACK_TALKER, seq, data, len, last);
        LeaveCriticalSection(&g_voiceLock);
        return;
    }
    EnterCriticalSection(&g_voiceLock);
    if (g_sendCount == VOICE_SENDQ) { g_sendHead = (g_sendHead + 1) % VOICE_SENDQ; g_sendCount--; }
    VoiceOut *o = &g_sendq[(g_sendHead + g_sendCount) % VOICE_SENDQ];
    o->seq = seq;
    o->len = (BYTE)len;
    o->last = (BYTE)last;
    o->made = GetTickCount();
    memcpy(o->data, data, len);
    g_sendCount++;
    LeaveCriticalSection(&g_voiceLock);
}

static void VoiceEncodeFrame(BOOL last)
{
    if (!g_enc) {
        int err = 0;
        g_enc = opus_encoder_create(VOICE_RATE, 1, OPUS_APPLICATION_VOIP, &err);
        if (!g_enc) { Log("voice: opus_encoder_create failed (%d)", err); return; }
        opus_encoder_ctl(g_enc, OPUS_SET_INBAND_FEC(1));
        opus_encoder_ctl(g_enc, OPUS_SET_PACKET_LOSS_PERC(10));
        opus_encoder_ctl(g_enc, OPUS_SET_DTX(1));
        opus_encoder_ctl(g_enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
        opus_encoder_ctl(g_enc, OPUS_SET_COMPLEXITY(8));
        g_encBitrate = 0;
    }
    int br = VoiceDvarInt(g_dvBitrate, 24000);
    if (br < 6000) br = 6000;
    if (br > 64000) br = 64000;
    if (br != g_encBitrate) { opus_encoder_ctl(g_enc, OPUS_SET_BITRATE(br)); g_encBitrate = br; }
    BYTE out[VOICE_MAXFRAME];
    int n = opus_encode_float(g_enc, g_capFrame, VOICE_FRAME, out, sizeof(out));
    if (n > 0) {
        VoiceQueueFrame(out, n, last);
        InterlockedIncrement(&g_statCapFrames);
    }
}

/* k33_voiceTone 1: transmit a 440 Hz test tone instead of the microphone (no capture device). */
static BOOL VoiceToneTick(BOOL want)
{
    static BOOL active;
    static DWORD start;
    static LONG64 made;
    static double phase;
    if (!g_dvTone || !VoiceDvarInt(g_dvTone, 0)) {
        if (active && !want) active = FALSE;
        if (!active) return FALSE;
    }
    if (want && !active) { active = TRUE; start = GetTickCount(); made = 0; g_capFill = 0; }
    if (!active) return TRUE;
    LONG64 due = (LONG64)(GetTickCount() - start) * VOICE_RATE / 1000;
    while (made + VOICE_FRAME <= due) {
        for (int i = 0; i < VOICE_FRAME; i++) {
            g_capFrame[i] = 0.25f * (float)sin(phase);
            phase += 2.0 * 3.14159265358979 * 440.0 / VOICE_RATE;
            if (phase > 6.283185307179586) phase -= 6.283185307179586;
        }
        VoiceEncodeFrame(FALSE);
        made += VOICE_FRAME;
    }
    if (!want) {
        for (int i = 0; i < VOICE_FRAME; i++) g_capFrame[i] = 0;
        VoiceEncodeFrame(TRUE);
        active = FALSE;
    }
    return TRUE;
}

static void VoiceCaptureTick(BOOL want)
{
    VoiceDev *d = &g_cap;
    if (VoiceToneTick(want)) return;
    if (want && !d->client) {
        if ((LONG)(GetTickCount() - d->retryAt) < 0) return;
        if (!VoiceDevOpen(d, TRUE)) { InterlockedExchange(&g_captureState, -1); return; }
        InterlockedExchange(&g_captureState, 1);
    }
    if (!d->client) return;
    if (want && !d->running) {
        g_capFill = 0;
        d->pos = 0;
        d->prev = 0;
        if (FAILED(IAudioClient_Start(d->client))) { VoiceDevRelease(d); d->retryAt = GetTickCount() + 3000; return; }
        d->running = TRUE;
        if (g_enc) opus_encoder_ctl(g_enc, OPUS_RESET_STATE);
    }
    if (!d->running) return;

    /* drain everything the device has */
    double step = (double)d->rate / VOICE_RATE;
    for (;;) {
        UINT32 packet = 0;
        HRESULT hr = IAudioCaptureClient_GetNextPacketSize(d->cap, &packet);
        if (FAILED(hr)) { Log("voice: capture lost (0x%08lX), reopening", (unsigned long)hr); VoiceDevRelease(d); d->retryAt = GetTickCount() + 2000; return; }
        if (!packet) break;
        BYTE *data;
        UINT32 frames;
        DWORD flags;
        if (FAILED(IAudioCaptureClient_GetBuffer(d->cap, &data, &frames, &flags, NULL, NULL))) break;
        for (UINT32 i = 0; i < frames; i++) {
            float x = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? 0.0f : VoiceSampleIn(d, data + i * d->blockAlign);
            /* linear resample to 48 kHz: emit every output sample that falls in [prev, x) */
            while (d->pos < 1.0) {
                g_capFrame[g_capFill++] = d->prev + (float)d->pos * (x - d->prev);
                if (g_capFill == VOICE_FRAME) { VoiceEncodeFrame(FALSE); g_capFill = 0; }
                d->pos += step;
            }
            d->pos -= 1.0;
            d->prev = x;
        }
        IAudioCaptureClient_ReleaseBuffer(d->cap, frames);
    }

    if (!want) {
        /* end of transmission: pad the partial frame and mark it last */
        for (int i = g_capFill; i < VOICE_FRAME; i++) g_capFrame[i] = 0;
        VoiceEncodeFrame(TRUE);
        g_capFill = 0;
        IAudioClient_Stop(d->client);
        IAudioClient_Reset(d->client);
        d->running = FALSE;
    }
}

/* ---- mix + playback --------------------------------------------------------------------------- */

static float g_mix[VOICE_FRAME], g_tmp[VOICE_FRAME];
static DWORD g_renderIdleSince;

static BOOL VoiceAnyQueued(void)
{
    for (int i = 0; i < VOICE_TALKERS; i++)
        if (g_talkers[i].queued || g_talkers[i].started) return TRUE;
    return FALSE;
}

static void VoiceRenderTick(void)
{
    VoiceDev *d = &g_ren;
    EnterCriticalSection(&g_voiceLock);
    BOOL any = VoiceAnyQueued();
    LeaveCriticalSection(&g_voiceLock);
    if (!d->client) {
        if (!any || (LONG)(GetTickCount() - d->retryAt) < 0) return;
        if (!VoiceDevOpen(d, FALSE)) { InterlockedExchange(&g_renderState, -1); return; }
        InterlockedExchange(&g_renderState, 1);
    }
    if (!d->running) {
        if (!any) return;
        d->pos = 0;
        d->prev = 0;
        if (FAILED(IAudioClient_Start(d->client))) { VoiceDevRelease(d); d->retryAt = GetTickCount() + 3000; return; }
        d->running = TRUE;
        g_renderIdleSince = 0;
    }
    if (any) g_renderIdleSince = 0;
    else if (!g_renderIdleSince) g_renderIdleSince = GetTickCount();
    else if (GetTickCount() - g_renderIdleSince > 3000) { /* idle: stop the stream until someone talks */
        IAudioClient_Stop(d->client);
        IAudioClient_Reset(d->client);
        d->running = FALSE;
        return;
    }

    float vol = VoiceDvarInt(g_dvVolume, 100) / 100.0f;
    double step = VOICE_RATE / (double)d->rate; /* input samples per output sample */
    UINT32 target = (UINT32)(d->rate * 60 / 1000); /* keep ~60 ms queued */
    if (target > d->bufferFrames) target = d->bufferFrames;
    for (int guard = 0; guard < 8; guard++) {
        UINT32 pad = 0;
        HRESULT hr = IAudioClient_GetCurrentPadding(d->client, &pad);
        if (FAILED(hr)) { Log("voice: playback lost (0x%08lX), reopening", (unsigned long)hr); VoiceDevRelease(d); d->retryAt = GetTickCount() + 2000; return; }
        /* one 20 ms mix frame yields about this many device frames */
        UINT32 chunk = (UINT32)((VOICE_FRAME - d->pos) / step) + 1;
        if (pad >= target || pad + chunk > d->bufferFrames) break;

        memset(g_mix, 0, sizeof(g_mix));
        int voices = 0;
        EnterCriticalSection(&g_voiceLock);
        for (int i = 0; i < VOICE_TALKERS; i++)
            if (g_talkers[i].queued || g_talkers[i].started) voices += VoicePull(&g_talkers[i], g_mix, g_tmp);
        LeaveCriticalSection(&g_voiceLock);
        if (voices) InterlockedIncrement(&g_statPlayed);

        /* resample 48 kHz -> device rate and write */
        UINT32 n = 0;
        double p = d->pos;
        while (p < VOICE_FRAME && n < chunk) { n++; p += step; }
        BYTE *buf;
        if (FAILED(IAudioRenderClient_GetBuffer(d->ren, n, &buf))) break;
        for (UINT32 i = 0; i < n; i++) {
            int k = (int)d->pos;
            float frac = (float)(d->pos - k);
            float a = k == 0 ? d->prev : g_mix[k - 1];
            float b = g_mix[k < VOICE_FRAME ? k : VOICE_FRAME - 1];
            VoiceSampleOut(d, buf + i * d->blockAlign, (a + frac * (b - a)) * vol);
            d->pos += step;
        }
        d->pos -= VOICE_FRAME;
        d->prev = g_mix[VOICE_FRAME - 1];
        IAudioRenderClient_ReleaseBuffer(d->ren, n, 0);
    }
}

static DWORD WINAPI VoiceThread(LPVOID unused)
{
    (void)unused;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    Log("voice: audio thread up (CoInitializeEx 0x%08lX)", (unsigned long)hr);
    timeBeginPeriod(1);
    for (;;) {
        BOOL want = g_wantCapture != 0;
        if (g_cap.running && g_cap.event) WaitForSingleObject(g_cap.event, 10);
        else Sleep(want ? 2 : 8);
        VoiceCaptureTick(want);
        VoiceRenderTick();
    }
    return 0;
}

/* ---- main thread: send, status ------------------------------------------------------------------ */

typedef BOOL(__cdecl *OobDataFn)(int sock, const BYTE *data, int len, DWORD a0, DWORD a1, DWORD a2, DWORD a3,
                                  DWORD a4, DWORD a5);

static const BYTE *VoiceServerAdr(void)
{
    if (*(volatile int *)(DWORD_PTR)VOICE_CONNSTATE_VA != VOICE_CA_ACTIVE) return NULL;
    DWORD enc = *(volatile DWORD *)(DWORD_PTR)VOICE_CLC_PTR_VA;
    if (!enc) return NULL;
    const BYTE *clc = (const BYTE *)(DWORD_PTR)(enc ^ VOICE_CLC_KEY);
    if (!Readable(clc + 0x14, 24)) return NULL;
    int type = *(const int *)(clc + 0x14);
    if (type < 2) return NULL; /* NA_BOT / NA_BAD */
    return clc + 0x14;
}

static void VoiceRegisterDvars(void)
{
    typedef const BYTE *(__cdecl *RegisterIntFn)(const char *, int, int, int, unsigned, const char *);
    RegisterIntFn reg = (RegisterIntFn)(DWORD_PTR)CON_REGISTER_INT_VA;
    g_voiceDvarsDone = TRUE;
    g_dvVoice = reg("codol_voice", 1, 0, 1, 0x1, "Voice chat: 1 on, 0 off (+set codol_voice 0 on the command line installs nothing)");
    g_dvKey = reg("k33_talkKey", 'V', 0, 255, 0x1,
                  "Push-to-talk key (Windows virtual-key code; 86 V, 20 Caps Lock, 0 none). Binding +talk also works.");
    g_dvVolume = reg("k33_voiceVolume", 100, 0, 400, 0x1, "Voice chat playback volume in percent");
    g_dvBitrate = reg("k33_voiceBitrate", 24000, 6000, 64000, 0x1, "Opus voice bitrate (bits per second)");
    g_dvLoopback = reg("k33_voiceLoopback", 0, 0, 1, 0, "Play your own voice back locally instead of sending it");
    g_dvDebug = reg("k33_voiceDebug", 0, 0, 2, 0, "Voice chat diagnostics in the console");
    g_dvTone = reg("k33_voiceTone", 0, 0, 1, 0, "Transmit a 440 Hz test tone instead of the microphone");

    /* C1 does not apply "+set" from the command line to a dvar registered this late (and runs no
     * "+command" at all), so re-issue ours: "+set k33_voiceLoopback 1", "+toggletalk". */
    static const char *const kMine[] = { "codol_voice", "k33_talkKey", "k33_voiceVolume", "k33_voiceBitrate",
                                         "k33_voiceLoopback", "k33_voiceDebug", "k33_voiceTone" };
    const char *cl = GetCommandLineA();
    for (unsigned i = 0; cl && i < sizeof(kMine) / sizeof(kMine[0]); i++) {
        char pat[64], cmd[96];
        _snprintf(pat, sizeof(pat), "+set %s ", kMine[i]);
        const char *at = strstr(cl, pat);
        if (!at) continue;
        at += strlen(pat);
        int v = atoi(at[0] == '"' ? at + 1 : at);
        _snprintf(cmd, sizeof(cmd), "set %s %d\n", kMine[i], v);
        if (g_conCbufOk) ConCbufAddText(cmd);
        Log("voice: command line: set %s %d", kMine[i], v);
    }
    if (cl && strstr(cl, "+toggletalk")) InterlockedExchange(&g_toggleTalk, 1);
}

static const char *VoiceKeyName(int vk, char *buf, int n)
{
    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (vk == VK_CAPITAL) return "Caps Lock";
    if (!sc || !GetKeyNameTextA((LONG)(sc << 16), buf, n)) _snprintf(buf, n, "virtual key %d", vk);
    return buf;
}

static BOOL VoiceEnabled(void) { return VoiceDvarInt(g_dvVoice, 1) != 0 && !g_uiVoiceOff; }

static void VoiceSendQueued(const BYTE *adr)
{
    BYTE pkt[2 + 5 + VOICE_PER_PACKET * (VOICE_MAXFRAME + 1)];
    for (;;) {
        EnterCriticalSection(&g_voiceLock);
        if (!g_sendCount) { LeaveCriticalSection(&g_voiceLock); return; }
        VoiceOut *first = &g_sendq[g_sendHead];
        BOOL anyLast = FALSE;
        int take = g_sendCount < VOICE_PER_PACKET ? g_sendCount : VOICE_PER_PACKET;
        for (int i = 0; i < take; i++) if (g_sendq[(g_sendHead + i) % VOICE_SENDQ].last) { take = i + 1; anyLast = TRUE; break; }
        /* batch two frames (40 ms) unless the transmission is ending or the frame is getting old */
        if (take < 2 && !anyLast && GetTickCount() - first->made < 30) { LeaveCriticalSection(&g_voiceLock); return; }
        int len = 0;
        pkt[len++] = 'v';
        pkt[len++] = 0;
        pkt[len++] = 1;
        pkt[len++] = anyLast ? 1 : 0;
        pkt[len++] = (BYTE)(first->seq & 0xFF);
        pkt[len++] = (BYTE)(first->seq >> 8);
        pkt[len++] = (BYTE)take;
        for (int i = 0; i < take; i++) {
            VoiceOut *o = &g_sendq[(g_sendHead + i) % VOICE_SENDQ];
            pkt[len++] = o->len;
            memcpy(pkt + len, o->data, o->len);
            len += o->len;
        }
        g_sendHead = (g_sendHead + take) % VOICE_SENDQ;
        g_sendCount -= take;
        LeaveCriticalSection(&g_voiceLock);
        if (adr) {
            const DWORD *a = (const DWORD *)adr;
            ((OobDataFn)(DWORD_PTR)VOICE_OOBDATA_VA)(0, pkt, len, a[0], a[1], a[2], a[3], a[4], a[5]);
            InterlockedIncrement(&g_statSent);
        }
    }
}

static void VoiceAnnounce(void)
{
    char kn[64];
    int vk = VoiceDvarInt(g_dvKey, 'V');
    g_voiceAnnounced = TRUE;
    ConPrintf("^3voice: Opus voice chat %s. Push-to-talk: hold %s (k33_talkKey %d)%s, or bind a key to +talk "
              "(e.g. bind CAPSLOCK +talk); toggletalk latches. k33voice shows status.\n",
              VoiceEnabled() ? "on" : "OFF (codol_voice 0)", vk ? VoiceKeyName(vk, kn, sizeof(kn)) : "no key", vk,
              g_voiceKeyOk ? "" : " [key hook OFF]");
}

/* Test automation, only with "+set k33_cmdFile 1" on the command line: every half second the lines
 * of <exe dir>\k33_cmd_<pid>.txt are executed (Cbuf) and the file is deleted. */
static void VoiceCmdFile(void)
{
    static int enabled = -1;
    static DWORD last;
    static char path[MAX_PATH];
    if (enabled < 0) {
        const char *cl = GetCommandLineA();
        enabled = cl && strstr(cl, "k33_cmdFile 1") != NULL;
        if (enabled) {
            DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
            while (n && path[n - 1] != '\\') n--;
            _snprintf(path + n, MAX_PATH - n, "k33_cmd_%lu.txt", GetCurrentProcessId());
            Log("voice: command file %s", path);
        }
    }
    if (!enabled || GetTickCount() - last < 500) return;
    last = GetTickCount();
    HANDLE h = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    char buf[2048];
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 2, &got, NULL);
    CloseHandle(h);
    DeleteFileA(path);
    buf[got] = 0;
    for (DWORD i = 0; i < got; i++)
        if (buf[i] == '\r') buf[i] = '\n';
    if (got && buf[got - 1] != '\n') { buf[got++] = '\n'; buf[got] = 0; }
    Log("voice: command file: %s", buf);
    if (g_conCbufOk) ConCbufAddText(buf);
}

/* Once per Com_EventLoop pass (main thread). */
__attribute__((used)) void __cdecl VoiceMainTick(void)
{
    static LONG ticks;
    if (++ticks == 1) Log("voice: first main-thread tick");
    if (!g_voiceDvarsDone) {
        if (*(volatile int *)(DWORD_PTR)CON_DVAR_COUNT_VA <= 100) return;
        VoiceRegisterDvars();
        Log("voice: dvars registered at tick %ld (codol_voice %d, k33_talkKey %d, loopback %d)", ticks,
            VoiceDvarInt(g_dvVoice, -1), VoiceDvarInt(g_dvKey, -1), VoiceDvarInt(g_dvLoopback, -1));
    }
    const BYTE *adr = VoiceServerAdr();
    BOOL on = VoiceEnabled();
    InterlockedExchange(&g_connected, adr != NULL);
    if (adr && !g_voiceAnnounced) VoiceAnnounce();

    BOOL talk = on && !g_micMuted && (g_keyTalk || g_cmdTalk || g_toggleTalk) &&
                (adr || VoiceDvarInt(g_dvLoopback, 0));
    if (talk != (g_wantCapture != 0)) {
        InterlockedExchange(&g_wantCapture, talk);
        if (VoiceDvarInt(g_dvDebug, 0) || g_lastTalkPrint < 3) {
            g_lastTalkPrint++;
            ConPrintf("voice: %s\n", talk ? "talking" : "stopped talking");
        }
    }
    if (g_captureState < 0 && talk) {
        static DWORD warned;
        if (!warned || GetTickCount() - warned > 30000) {
            warned = GetTickCount();
            ConPrintf("^1voice: no microphone could be opened (see Kernel33.log)\n");
        }
    }
    if (VoiceDvarInt(g_dvDebug, 0)) {
        static DWORD lastStat;
        if (GetTickCount() - lastStat > 5000) { lastStat = GetTickCount(); VoiceCmdStatus(); }
    }
    VoiceCmdFile();
    if (adr && on) VoiceSendQueued(adr);
    else if (g_sendCount) {
        EnterCriticalSection(&g_voiceLock);
        g_sendCount = 0;
        LeaveCriticalSection(&g_voiceLock);
    }
}

/* CL_ConnectionlessPacket's "v" branch (main thread). msg: data +8, cursize +0x14. */
__attribute__((used)) void __cdecl VoiceOnPacket(const BYTE *msg, const DWORD *from)
{
    if (!msg || !Readable(msg, 0x18) || !g_voiceDvarsDone || !VoiceEnabled()) return;
    const BYTE *data = *(const BYTE *const *)(msg + 8);
    int size = *(const int *)(msg + 0x14);
    if (!data || size < 4 + 2 + 6 || size > 0x20000 || !Readable(data, size)) return;
    const BYTE *adr = VoiceServerAdr();
    if (!adr || !from) return;
    const DWORD *s = (const DWORD *)adr;
    /* type, ip, port (+ the first ipx bytes) must be the server's */
    if (from[0] != s[0] || from[1] != s[1] || (from[2] & 0xFFFF) != (s[2] & 0xFFFF)) return;
    const BYTE *p = data + 6; /* ff ff ff ff 'v' 0 */
    int avail = size - 6;
    if (data[4] != 'v' || data[5] != 0 || avail < 6 || p[0] != 1) return;
    int flags = p[1], talker = p[2], count = p[5];
    unsigned short seq = (unsigned short)(p[3] | (p[4] << 8));
    if (talker >= VOICE_TALKERS || count < 1 || count > 8) return;
    int off = 6;
    InterlockedIncrement(&g_statRecv);
    EnterCriticalSection(&g_voiceLock);
    for (int i = 0; i < count; i++) {
        if (off >= avail) break;
        int len = p[off];
        if (len < 1 || off + 1 + len > avail) break;
        VoiceInsert(talker, (unsigned short)(seq + i), p + off + 1, len, (flags & 1) && i == count - 1);
        off += 1 + len;
    }
    LeaveCriticalSection(&g_voiceLock);
    if (VoiceDvarInt(g_dvDebug, 0) >= 2)
        ConPrintf("voice: rx talker %d seq %u frames %d flags %d\n", talker, seq, count, flags);
}

/* ---- commands --------------------------------------------------------------------------------- */

static int VoiceArgc(void)
{
    int idx = *(volatile int *)(DWORD_PTR)VOICE_CMD_ARGS_VA;
    if (idx < 0 || idx > 7) return 0;
    return *(volatile int *)(DWORD_PTR)(VOICE_CMD_ARGS_VA + 0x44 + idx * 4);
}

static const char *VoiceArgv(int n)
{
    int idx = *(volatile int *)(DWORD_PTR)VOICE_CMD_ARGS_VA;
    if (idx < 0 || idx > 7 || n >= VoiceArgc()) return "";
    const char *const *argv = *(const char *const *const *)(DWORD_PTR)(VOICE_CMD_ARGS_VA + 0x64 + idx * 4);
    return argv && Readable(argv + n, 4) && argv[n] ? argv[n] : "";
}

static void __cdecl VoiceCmdTalkDown(void) { InterlockedExchange(&g_cmdTalk, 1); }
static void __cdecl VoiceCmdTalkUp(void) { InterlockedExchange(&g_cmdTalk, 0); }
static void __cdecl VoiceCmdToggle(void)
{
    Log("voice: toggletalk command");
    LONG v = !g_toggleTalk;
    InterlockedExchange(&g_toggleTalk, v);
    ConPrintf("voice: toggletalk %s\n", v ? "on (transmitting)" : "off");
}

static void __cdecl VoiceCmdMute(void)
{
    if (VoiceArgc() < 2) {
        ConPrintf("usage: k33voicemute <client number>   (toggles; k33voice lists who is talking)\n");
        return;
    }
    int n = atoi(VoiceArgv(1));
    if (n < 0 || n >= VOICE_TALKERS) return;
    LONG m = !g_talkers[n].muted;
    InterlockedExchange(&g_talkers[n].muted, m);
    ConPrintf("voice: client %d %s\n", n, m ? "muted" : "unmuted");
}

static void __cdecl VoiceCmdStatus(void)
{
    char kn[64];
    int vk = VoiceDvarInt(g_dvKey, 'V');
    ConPrintf("voice: %s, %s, mic %s%s; key %s (k33_talkKey %d), +talk %s, toggletalk %s, loopback %d, volume %d%%, "
              "bitrate %d\n",
              VoiceEnabled() ? "on" : "OFF", g_connected ? "connected" : "not in a match",
              g_captureState > 0 ? g_captureName : g_captureState < 0 ? "FAILED" : "not opened yet",
              g_micMuted ? " (muted)" : "", vk ? VoiceKeyName(vk, kn, sizeof(kn)) : "none", vk,
              g_voiceCmdsOk ? "registered" : "UNAVAILABLE", g_toggleTalk ? "on" : "off", VoiceDvarInt(g_dvLoopback, 0),
              VoiceDvarInt(g_dvVolume, 100), VoiceDvarInt(g_dvBitrate, 24000));
    ConPrintf("voice: playback %s; frames captured %ld, packets sent %ld, received %ld, mixed %ld, concealed %ld\n",
              g_renderState > 0 ? g_renderName : g_renderState < 0 ? "FAILED" : "not opened yet", g_statCapFrames,
              g_statSent, g_statRecv, g_statPlayed, g_statLostFrames);
    DWORD now = GetTickCount();
    for (int i = 0; i < VOICE_TALKERS; i++)
        if (g_lastHeard[i] && now - g_lastHeard[i] < 60000)
            ConPrintf("voice:   client %d last heard %lu ms ago%s%s\n", i, now - g_lastHeard[i],
                      now - g_lastHeard[i] < 500 ? " (talking)" : "", g_talkers[i].muted ? " [muted]" : "");
}

/* Test automation: "k33ready [0|1]" presses the room's Ready button the way the Scaleform
 * SetReadyState callback does (AS_SetReadyState::Call 0x6AEFF0 reads the bool at args[0]+8). Only
 * with "+set k33_cmdFile 1" on the command line. */
static void __cdecl VoiceCmdReady(void)
{
    static const BYTE kReady[] = { 0x55, 0x8B, 0xEC, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x84, 0x05, 0xA8, 0xB0, 0x2E, 0x08 };
    const char *cl = GetCommandLineA();
    if (!cl || !strstr(cl, "k33_cmdFile 1")) { ConPrintf("k33ready: test builds only (+set k33_cmdFile 1)\n"); return; }
    if (!Readable((void *)(DWORD_PTR)0x006AEFF0u, sizeof(kReady)) || memcmp((void *)(DWORD_PTR)0x006AEFF0u, kReady, sizeof(kReady))) return;
    static double args[2];
    static DWORD params[8];
    memset(args, 0, sizeof(args));
    ((BYTE *)args)[4] = 2; /* bool */
    ((BYTE *)args)[8] = VoiceArgc() < 2 || atoi(VoiceArgv(1)) != 0;
    params[4] = (DWORD)(DWORD_PTR)args;
    params[5] = 1;
    typedef void(__attribute__((thiscall)) * CallFn)(void *, void *);
    ((CallFn)(DWORD_PTR)0x006AEFF0u)(NULL, params);
    Log("voice: k33ready %d", ((BYTE *)args)[8]);
}

typedef struct VoiceCmdNode { struct VoiceCmdNode *next; const char *name; void *fn; } VoiceCmdNode;
static VoiceCmdNode g_voiceCmds[] = {
    { NULL, "+talk", (void *)VoiceCmdTalkDown },   { NULL, "-talk", (void *)VoiceCmdTalkUp },
    { NULL, "toggletalk", (void *)VoiceCmdToggle }, { NULL, "k33voice", (void *)VoiceCmdStatus },
    { NULL, "k33voicemute", (void *)VoiceCmdMute }, { NULL, "k33ready", (void *)VoiceCmdReady },
};

/* Link the nodes into C1's command list. Done before the game entry: the list head 0x24F6424 is
 * plain zero-initialised data that Cmd_AddCommand (inlined, e.g. 0x498BA9) only prepends to, so the
 * commands exist before the command line and config_mp.cfg run ("bind CAPSLOCK +talk" works). */
static void VoiceRegisterCommands(void)
{
    static BOOL done;
    if (done) return;
    done = TRUE;
    VoiceCmdNode **head = (VoiceCmdNode **)(DWORD_PTR)CON_CMD_LIST_VA;
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_voiceCmds) / sizeof(g_voiceCmds[0]); i++) {
        BOOL exists = FALSE;
        for (VoiceCmdNode *c = *head; c && Readable(c, 12); c = c->next)
            if (c->name && Readable(c->name, 1) && !_stricmp(c->name, g_voiceCmds[i].name)) { exists = TRUE; break; }
        if (exists) { Log("voice: command %s already exists, left alone", g_voiceCmds[i].name); continue; }
        g_voiceCmds[i].next = *head;
        *head = &g_voiceCmds[i];
        added++;
    }
    g_voiceCmdsOk = added > 0;
    Log("voice: %d commands linked into the command list", added);
}

/* ---- WndProc key path (main thread) ---------------------------------------------------------- */

/* 1 when the message belongs to push-to-talk and must not reach the game. The talk key only acts
 * in play: while a menu, the chat line or the console owns the keyboard (keyCatchers [0x16FC2E0],
 * the word CL_CmdButtons 0x4905C5 tests; or the Kernel33 console) it types normally. While it is
 * held for talking, its key-down and key-up are kept from C1, so a bind on the same key never
 * fires as well. */
#define VOICE_KEYCATCHERS_VA 0x016FC2E0u
static BOOL VoiceWndMsg(UINT msg, WPARAM wp, LPARAM lp)
{
    if (!g_voiceOn) return FALSE;
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (!g_voiceDvarsDone) return FALSE;
        int vk = VoiceDvarInt(g_dvKey, 'V');
        if (vk <= 0 || (int)wp != vk) return FALSE;
        if (g_keyTalk) return TRUE; /* auto-repeat while talking */
        if (g_conOpen || *(volatile int *)(DWORD_PTR)VOICE_KEYCATCHERS_VA) return FALSE;
        InterlockedExchange(&g_keyTalk, 1);
        return TRUE;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        int vk = VoiceDvarInt(g_dvKey, 'V');
        if (vk > 0 && (int)wp == vk && g_keyTalk) { InterlockedExchange(&g_keyTalk, 0); return TRUE; }
        return FALSE;
    }
    case WM_KILLFOCUS:
        InterlockedExchange(&g_keyTalk, 0);
        InterlockedExchange(&g_cmdTalk, 0);
        break;
    case WM_ACTIVATEAPP:
        if (!wp) { InterlockedExchange(&g_keyTalk, 0); InterlockedExchange(&g_cmdTalk, 0); }
        break;
    }
    return FALSE;
}

static LRESULT CALLBACK VoiceWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (VoiceWndMsg(msg, wp, lp)) return 0;
    return ((WNDPROC)(DWORD_PTR)GAME_WNDPROC_VA)(hwnd, msg, wp, lp);
}

/* ---- Scaleform callbacks ---------------------------------------------------------------------- */

/* GFx FunctionHandler::Params: pArgs +0x10, ArgCount +0x14; GFxValue 16 bytes {iface, type, value +8}
 * (C1's own handlers read [[params+0x10]+8], e.g. 0x6AF396). Type & 0x8F: 2 bool, 3 number. */
static int VoiceAsArg(const BYTE *params, int *value)
{
    if (!params || !Readable(params, 0x18)) return 0;
    const BYTE *args = *(const BYTE *const *)(params + 0x10);
    unsigned count = *(const unsigned *)(params + 0x14);
    if (!count || !args || !Readable(args, 16)) return 0;
    int type = *(const int *)(args + 4) & 0x8F;
    if (type == 2) { *value = args[8] != 0; return 1; }
    if (type == 3) { *value = *(const double *)(args + 8) != 0.0; return 1; }
    return 0;
}

__attribute__((thiscall)) static void VoiceAsEnable(void *self, const BYTE *params)
{
    (void)self;
    int v = 1;
    int has = VoiceAsArg(params, &v);
    Log("voice: UI EnableVoiceTalk(%s)", has ? (v ? "true" : "false") : "?");
    if (has) InterlockedExchange(&g_uiVoiceOff, !v);
}

__attribute__((thiscall)) static void VoiceAsMicMute(void *self, const BYTE *params)
{
    (void)self;
    int v = 0;
    int has = VoiceAsArg(params, &v);
    Log("voice: UI SetVoiceMicMute(%s)", has ? (v ? "true" : "false") : "?");
    if (has) InterlockedExchange(&g_micMuted, v);
}

/* ---- thunks and install --------------------------------------------------------------------------- */

__attribute__((used)) DWORD g_voiceEventLoop = VOICE_EVENTLOOP_FUNC_VA;
__attribute__((used)) DWORD g_voiceRecvDone = VOICE_RECV_DONE_VA;
void VOICE_TickHook(void);
void VOICE_RecvHook(void);
__asm__(
    ".text\n"
    ".globl _VOICE_TickHook\n"
    "_VOICE_TickHook:\n"              /* call 0x595CF0 at 0x595E9D: tick, then the original */
    "    pushal\n"
    "    call _VoiceMainTick\n"
    "    popal\n"
    "    jmp *_g_voiceEventLoop\n"
    ".globl _VOICE_RecvHook\n"
    "_VOICE_RecvHook:\n"              /* jmp from 0x496482 (CL_ConnectionlessPacket, "v") */
    "    pushal\n"
    "    leal 0x10(%ebp), %eax\n"     /* from (netadr_t) */
    "    pushl %eax\n"
    "    pushl -0xC6C(%ebp)\n"        /* msg */
    "    call _VoiceOnPacket\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    jmp *_g_voiceRecvDone\n");     /* mov al, 1 ... ret */

static BOOL VoiceSlotHook(DWORD slot, void *fn)
{
    DWORD *p = (DWORD *)(DWORD_PTR)slot;
    if (!Readable(p, 4) || *p != VOICE_AS_NOP) return FALSE;
    DWORD v = (DWORD)(DWORD_PTR)fn;
    WriteCode(p, &v, 4);
    return TRUE;
}

static void InstallVoice(BOOL consoleWndProc)
{
    static const BYTE kRecv[] = { 0x68, 0x7C, 0x7E, 0xC4, 0x00, 0x6A, 0x10, 0xE8 };     /* push msg; push 10h; call */
    static const BYTE kRecvDone[] = { 0xB0, 0x01, 0x5F, 0x5E, 0x5B };                  /* mov al,1; pop edi/esi/ebx */
    static const BYTE kRecvMsg[] = { 0x89, 0x8D, 0x94, 0xF3, 0xFF, 0xFF };             /* mov [ebp-0C6Ch], ecx @0x496421 */
    static const BYTE kOob[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53 };
    static const BYTE kClc[] = { 0x8B, 0x3D, 0x68, 0x80, 0x6F, 0x01, 0x6A, 0x01, 0x81, 0xF7, 0xCC, 0x50, 0xF9, 0x26 };
    static const BYTE kWndClass[] = { 0xC7, 0x45, 0xD8, 0x90, 0x35, 0x63, 0x00 };

    const char *cl = GetCommandLineA();
    if (cl && (strstr(cl, "codol_voice 0") || strstr(cl, "codol_voice \"0\""))) {
        Log("voice: off (codol_voice 0)");
        return;
    }
    InitializeCriticalSection(&g_voiceLock);

    /* send: NET_OutOfBandData + clc.serverAddress */
    g_voiceSendOk = Readable((void *)(DWORD_PTR)VOICE_OOBDATA_VA, sizeof(kOob)) &&
                    !memcmp((void *)(DWORD_PTR)VOICE_OOBDATA_VA, kOob, sizeof(kOob)) &&
                    Readable((void *)(DWORD_PTR)0x00496729u, sizeof(kClc)) &&
                    !memcmp((void *)(DWORD_PTR)0x00496729u, kClc, sizeof(kClc));
    BOOL tickOk = g_voiceSendOk && CallsTarget(VOICE_EVENTLOOP_CALL_VA, VOICE_EVENTLOOP_FUNC_VA);
    if (tickOk) RedirectCall(VOICE_EVENTLOOP_CALL_VA, (void *)VOICE_TickHook);

    /* receive */
    BYTE *recv = (BYTE *)(DWORD_PTR)VOICE_RECV_VA;
    g_voiceRecvOk = Readable(recv, sizeof(kRecv)) && !memcmp(recv, kRecv, sizeof(kRecv)) &&
                    !memcmp((void *)(DWORD_PTR)VOICE_RECV_DONE_VA, kRecvDone, sizeof(kRecvDone)) &&
                    !memcmp((void *)(DWORD_PTR)0x00496421u, kRecvMsg, sizeof(kRecvMsg));
    if (g_voiceRecvOk) {
        BYTE jmp[5] = { 0xE9 };
        LONG rel = (LONG)((DWORD_PTR)VOICE_RecvHook - (VOICE_RECV_VA + 5));
        memcpy(jmp + 1, &rel, 4);
        WriteCode(recv, jmp, 5);
    }

    /* keys: the console's WndProc forwards to VoiceWndMsg; without the console, hook it here */
    if (consoleWndProc) g_voiceKeyOk = TRUE;
    else {
        BYTE *wc = (BYTE *)(DWORD_PTR)CON_WNDCLASS_VA;
        if (Readable(wc, sizeof(kWndClass)) && !memcmp(wc, kWndClass, sizeof(kWndClass))) {
            DWORD proc = (DWORD)(DWORD_PTR)VoiceWndProc;
            WriteCode(wc + 3, &proc, 4);
            g_voiceKeyOk = TRUE;
        }
    }

    /* commands */
    static const BYTE kCmdFind[] = { 0xA1, 0x24, 0x64, 0x4F, 0x02, 0x53, 0x85, 0xC0 };
    if (Readable((void *)(DWORD_PTR)CON_CMDFIND_VA, sizeof(kCmdFind)) &&
        !memcmp((void *)(DWORD_PTR)CON_CMDFIND_VA, kCmdFind, sizeof(kCmdFind)))
        VoiceRegisterCommands();
    else
        Log("voice: Cmd_FindCommand bytes differ, +talk/-talk not registered (k33_talkKey still works)");

    BOOL asEnable = VoiceSlotHook(VOICE_AS_ENABLE_SLOT, (void *)VoiceAsEnable);
    BOOL asMute = VoiceSlotHook(VOICE_AS_MICMUTE_SLOT, (void *)VoiceAsMicMute);

    g_voiceOn = tickOk || g_voiceRecvOk;
    if (g_voiceOn) {
        HANDLE t = CreateThread(NULL, 0, VoiceThread, NULL, 0, NULL);
        if (t) { SetThreadPriority(t, THREAD_PRIORITY_ABOVE_NORMAL); CloseHandle(t); }
        else g_voiceOn = FALSE;
    }
    Log("voice: send %s, tick %s, receive %s, keys %s, UI EnableVoiceTalk %s, SetVoiceMicMute %s, opus %s%s",
        g_voiceSendOk ? "ok" : "bytes differ", tickOk ? "hooked (0x595E9D)" : "NOT hooked",
        g_voiceRecvOk ? "hooked (0x496482)" : "NOT hooked (bytes differ)", g_voiceKeyOk ? "ok" : "NOT hooked",
        asEnable ? "hooked" : "bytes differ", asMute ? "hooked" : "bytes differ", opus_get_version_string(),
        g_voiceOn ? "" : " -- voice OFF");
}
