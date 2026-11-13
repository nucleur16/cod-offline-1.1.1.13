/*
 * In-game voice chat for C1. Included by kernel33.c after c1game.c, so it shares
 * Log/Readable/WriteCode/CallsTarget/RedirectCall and c1game.c's C1 print/Cbuf/command helpers.
 *
 * C1 stripped IW's voice: no capture (no DirectSound/waveIn), no codec (the Speex tables are gone),
 * no CL_WriteVoicePacket, no +talk. What is left: the "v" branch of CL_ConnectionlessPacket (it only
 * prints "voice communication is disabled"), the voice dvars, two Scaleform callbacks that return
 * at once, and the head-icon talk balloon. This file puts the rest back:
 *
 *   capture  WASAPI shared mode, default capture endpoint, on a worker thread; converted to 48 kHz
 *            mono and encoded with libopus (VOIP, 20 ms frames, FEC, DTX), linked statically.
 *            Push-to-talk (k33_voiceMode 0): the stream only runs while +talk is held. Voice
 *            activation (k33_voiceMode 1): it stays open in a match and a level gate (threshold,
 *            attack, hang, 120 ms pre-roll; see "capture + encode") decides what is sent.
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
 *   keys     +talk/-talk are a C1 key binding like +melee (see "push-to-talk as a C1 bind"): added to
 *            the bind table, run by the binding executor, rebound in Options > Controls > Chat and
 *            saved in config_mp.cfg; default V when nothing is bound to +talk and V is free.
 *            +talk/-talk/toggletalk are also console commands in the cmd list 0x24F6424.
 *   ui       AS_EnableVoiceTalk (vtable 0xC77100) and AS_SetVoiceMicMute (0xC636A8) Call slots,
 *            both `ret 4` at 0x97C8B0, go to VoiceAsEnable / VoiceAsMicMute.
 *
 * Wire format (see CoD4x src/codol/net/codol_voice.c, the relay):
 *   client -> server  "\xff\xff\xff\xffv\0" u8 ver=1, u8 flags (1 = last), u16 seq, u8 n, n*(u8 len, data)
 *   server -> client  "\xff\xff\xff\xffv\0" u8 ver=1, u8 flags, u8 talker, u16 seq, u8 n, frames
 *
 * Dvars: codol_voice (master switch; "+set codol_voice 0" installs nothing),
 * k33_voiceVolume (percent), k33_voiceBitrate, k33_voiceLoopback (hear yourself locally, no server),
 * k33_voiceDebug, k33_voiceTone, k33_voiceMode (0 push-to-talk, 1 voice activation), k33_voiceThreshold
 * (0..100 = -60..0 dBFS), k33_voiceHangMs, k33_voiceMicMute, k33_voiceLevel (live, read only),
 * k33_voiceMeter (Options > Audio's meter request). Commands: +talk, -talk, toggletalk, k33voice
 * (status), k33voicemute <client>.
 * Options > Audio's Voice section (voice_ui_mp: zones/voice_ui_mp/.../as/codo/views/option_sub/OptionView_audio.as) sets these.
 *
 * Every site is byte-checked; a mismatch is logged and that part stays off. Missing audio devices
 * only disable capture or playback (retried every few seconds), never the game.
 */

#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <ctype.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propvarutil.h>
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
#define VOICE_FRAME_MS  20
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
static BOOL g_voiceSendOk, g_voiceRecvOk, g_voiceCmdsOk;
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
static const BYTE *volatile g_dvVoice, *volatile g_dvVolume, *volatile g_dvBitrate,
    *volatile g_dvLoopback, *volatile g_dvDebug, *volatile g_dvTone, *volatile g_dvMode, *volatile g_dvThreshold,
    *volatile g_dvHang, *volatile g_dvMicMute, *volatile g_dvLevel, *volatile g_dvMeter,
    *volatile g_dvInDevice, *volatile g_dvInList, *volatile g_dvInPick, *volatile g_dvInIndex;
static BOOL g_voiceDvarsDone, g_voiceAnnounced;
static unsigned short g_sendSeq;
static volatile DWORD g_lastHeard[VOICE_TALKERS];
static int g_lastTalkPrint;

static void __cdecl VoiceCmdStatus(void);
static void VoiceTalkDown(int key);
static void VoiceTalkUp(int key);
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
#if CODOL_DIAGNOSTICS
        InterlockedIncrement(&g_statLostFrames);
#endif
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

/* ---- microphone choice (k33_voiceInputDevice) -------------------------------------------------------
 * The audio thread lists the active capture endpoints (EnumAudioEndpoints eCapture, DEVICE_STATE_ACTIVE,
 * PKEY_Device_FriendlyName) when it starts and whenever Windows reports a device change
 * (IMMNotificationClient), into g_inIds / g_inNames (names in code page 936, what C1 shows; a character
 * GBK lacks becomes '?'). The main thread publishes the names as k33_voiceInputList ("Default;name;..."),
 * the chosen one's position as k33_voiceInputIndex, and turns Options > Audio's pick (k33_voiceInputPick,
 * an index) into k33_voiceInputDevice (archived: the endpoint ID; empty = Windows' default
 * communications device). The capture stream is reopened on the chosen endpoint as soon as the choice
 * changes; a chosen device that is missing falls back to the default (logged), and is taken again when it
 * comes back. */
#define VOICE_MAX_INPUTS 16
static char g_inIds[VOICE_MAX_INPUTS][160], g_inNames[VOICE_MAX_INPUTS][96];
static volatile LONG g_inCount, g_inListGen;
static char g_inWant[160];               /* the chosen endpoint ID (main thread writes, audio thread reads) */
static volatile LONG g_inWantGen;
static char g_capOpenedId[160];          /* what the capture stream was opened on ("" = default) */
static volatile LONG g_capFallback;      /* opened on the default because the chosen one was missing */
static volatile LONG g_devDirty = 1;

static const PROPERTYKEY kPkeyFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };
static const GUID kIidNotify = { 0x7991EEC9, 0x7E89, 0x4D85, { 0x83, 0x90, 0x6C, 0x70, 0x3C, 0xEC, 0x60, 0xC0 } };
static const GUID kIidUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

/* IMMNotificationClient: any change marks the list dirty; the audio thread does the work. */
static HRESULT STDMETHODCALLTYPE VnQI(IMMNotificationClient *t, REFIID r, void **o)
{
    if (IsEqualGUID(r, &kIidUnknown) || IsEqualGUID(r, &kIidNotify)) { *o = t; return S_OK; }
    *o = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE VnRef(IMMNotificationClient *t) { (void)t; return 1; }
static HRESULT STDMETHODCALLTYPE VnState(IMMNotificationClient *t, LPCWSTR id, DWORD st) { (void)t; (void)id; (void)st; InterlockedExchange(&g_devDirty, 1); return S_OK; }
static HRESULT STDMETHODCALLTYPE VnAdd(IMMNotificationClient *t, LPCWSTR id) { (void)t; (void)id; InterlockedExchange(&g_devDirty, 1); return S_OK; }
static HRESULT STDMETHODCALLTYPE VnDefault(IMMNotificationClient *t, EDataFlow f, ERole r, LPCWSTR id) { (void)t; (void)f; (void)r; (void)id; InterlockedExchange(&g_devDirty, 1); return S_OK; }
static HRESULT STDMETHODCALLTYPE VnProp(IMMNotificationClient *t, LPCWSTR id, const PROPERTYKEY k) { (void)t; (void)id; (void)k; return S_OK; }
static IMMNotificationClientVtbl g_vnVtbl = { VnQI, VnRef, VnRef, VnState, VnAdd, VnAdd, VnDefault, VnProp };
static IMMNotificationClient g_vnClient = { &g_vnVtbl };

static void VoiceListInputs(void)
{
    static IMMDeviceEnumerator *watch;
    IMMDeviceEnumerator *en = NULL;
    IMMDeviceCollection *col = NULL;
    UINT n = 0;
    if (FAILED(CoCreateInstance(&kClsidEnum, NULL, CLSCTX_ALL, &kIidEnum, (void **)&en))) return;
    if (!watch) {   /* kept for the notifications */
        watch = en;
        IMMDeviceEnumerator_AddRef(watch);
        IMMDeviceEnumerator_RegisterEndpointNotificationCallback(watch, &g_vnClient);
    }
    char ids[VOICE_MAX_INPUTS][160], names[VOICE_MAX_INPUTS][96];
    int count = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eCapture, DEVICE_STATE_ACTIVE, &col)) &&
        SUCCEEDED(IMMDeviceCollection_GetCount(col, &n))) {
        for (UINT i = 0; i < n && count < VOICE_MAX_INPUTS; i++) {
            IMMDevice *dev = NULL;
            LPWSTR wid = NULL;
            IPropertyStore *ps = NULL;
            if (FAILED(IMMDeviceCollection_Item(col, i, &dev))) continue;
            if (SUCCEEDED(IMMDevice_GetId(dev, &wid)) && wid) {
                WideCharToMultiByte(CP_ACP, 0, wid, -1, ids[count], sizeof(ids[count]), NULL, NULL);
                CoTaskMemFree(wid);
                lstrcpynA(names[count], "?", sizeof(names[count]));
                if (SUCCEEDED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &ps))) {
                    PROPVARIANT v;
                    PropVariantInit(&v);
                    if (SUCCEEDED(IPropertyStore_GetValue(ps, &kPkeyFriendlyName, &v)) && v.vt == VT_LPWSTR && v.pwszVal) {
                        BOOL bad = FALSE;
                        WideCharToMultiByte(936, 0, v.pwszVal, -1, names[count], sizeof(names[count]), "?", &bad);
                        names[count][sizeof(names[count]) - 1] = 0;
                        for (char *c = names[count]; *c; c++)   /* the list's separators and the Cbuf's quotes */
                            if (*c == ';' || *c == '"' || *c == '|') *c = ' ';
                    }
                    PropVariantClear(&v);
                    IPropertyStore_Release(ps);
                }
                count++;
            }
            IMMDevice_Release(dev);
        }
    }
    if (col) IMMDeviceCollection_Release(col);
    IMMDeviceEnumerator_Release(en);
    EnterCriticalSection(&g_voiceLock);
    memcpy(g_inIds, ids, sizeof(ids[0]) * count);
    memcpy(g_inNames, names, sizeof(names[0]) * count);
    InterlockedExchange(&g_inCount, count);
    LeaveCriticalSection(&g_voiceLock);
    InterlockedIncrement(&g_inListGen);
    Log("voice: %d capture device(s)", count);
    for (int i = 0; i < count; i++) Log("voice:   %s  %s", ids[i], names[i]);
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
    hr = E_FAIL;
    if (capture) {   /* the chosen microphone, if any and present */
        char want[160];
        EnterCriticalSection(&g_voiceLock);
        lstrcpynA(want, g_inWant, sizeof(want));
        LeaveCriticalSection(&g_voiceLock);
        lstrcpynA(g_capOpenedId, "", sizeof(g_capOpenedId));
        InterlockedExchange(&g_capFallback, 0);
        if (want[0]) {
            WCHAR wid[160];
            DWORD st = 0;
            MultiByteToWideChar(CP_ACP, 0, want, -1, wid, 160);
            hr = IMMDeviceEnumerator_GetDevice(en, wid, &dev);
            if (SUCCEEDED(hr) && (FAILED(IMMDevice_GetState(dev, &st)) || st != DEVICE_STATE_ACTIVE)) {
                IMMDevice_Release(dev);
                dev = NULL;
                hr = E_FAIL;
            }
            if (SUCCEEDED(hr)) lstrcpynA(g_capOpenedId, want, sizeof(g_capOpenedId));
            else {
                Log("voice: capture: the chosen device %s is not available, using the default", want);
                InterlockedExchange(&g_capFallback, 1);
            }
        }
    }
    if (FAILED(hr)) hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, capture ? eCapture : eRender, eCommunications, &dev);
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

/* ---- capture + encode ----------------------------------------------------------------------- *
 *
 * The main thread decides what the microphone is for (VoiceMainTick): g_wantCapture opens the
 * stream, g_txForce transmits regardless of level (push-to-talk held, toggletalk, the +talk override
 * in voice-activation mode), g_vadArmed lets the voice-activation gate transmit, and with neither set
 * the stream only meters (the Options > Audio Input Level). Every 20 ms frame goes through
 * VoiceProcessFrame on the audio thread:
 *
 *   level   RMS of the frame in dBFS, mapped -60..0 dBFS -> 0..100 (k33_voiceThreshold's scale).
 *   attack  the gate opens after two frames in a row at or above the threshold (40 ms), or at once
 *           on a frame 15 above it (a loud onset).
 *   hang    every frame within 8 of the threshold (about 5 dB hysteresis) restarts k33_voiceHangMs
 *           (400); when it runs out the gate closes and that frame goes out marked last.
 *   pre-roll  the last VOICE_PREROLL frames (120 ms) before the gate opened are encoded first, so
 *           the start of a word that only crossed the threshold half way through is not clipped.
 *
 * Opus DTX stays on while transmitting; the encoder is reset at the start of each talk spurt. */

#define VOICE_PREROLL 6        /* frames of pre-roll (120 ms) */

static volatile LONG g_micLevel;      /* peak of the last 20 ms frame, 0..1000 (HUD brightness) */
static volatile LONG g_micLevel100;   /* RMS level of the last frame, 0..100 = -60..0 dBFS */
static volatile LONG g_txForce, g_vadArmed; /* main thread -> audio thread */
static volatile LONG g_vadThreshold = 30, g_vadHangMs = 400;
static volatile LONG g_txActive;      /* audio thread: a talk spurt is being sent */
static volatile LONG g_statSpurts;
static OpusEncoder *g_enc;
static int g_encBitrate;
static float g_capFrame[VOICE_FRAME];
static int g_capFill;
static float g_preroll[VOICE_PREROLL][VOICE_FRAME];
static int g_prerollHead, g_prerollCount;
static int g_vadAbove, g_vadHangLeft;
static BOOL g_vadOpen;

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

static void VoiceEncodePcm(const float *pcm, BOOL last)
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
    int n = opus_encode_float(g_enc, pcm, VOICE_FRAME, out, sizeof(out));
    if (n > 0) {
        VoiceQueueFrame(out, n, last);
#if CODOL_DIAGNOSTICS
        InterlockedIncrement(&g_statCapFrames);
#endif
    }
}

/* 0..100 for -60..0 dBFS; also sets the HUD's peak level. */
static int VoiceFrameLevel(const float *pcm)
{
    float peak = 0;
    double sum = 0;
    for (int i = 0; i < VOICE_FRAME; i++) {
        float a = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (a > peak) peak = a;
        sum += (double)pcm[i] * pcm[i];
    }
    InterlockedExchange(&g_micLevel, (LONG)(peak > 1.0f ? 1000 : peak * 1000));
    double rms = sqrt(sum / VOICE_FRAME);
    double db = rms > 1e-6 ? 20.0 * log10(rms) : -120.0;
    int level = (int)floor((db + 60.0) * 100.0 / 60.0 + 0.5);
    return level < 0 ? 0 : level > 100 ? 100 : level;
}

static void VoiceGateReset(void)
{
    g_prerollHead = g_prerollCount = 0;
    g_vadAbove = g_vadHangLeft = 0;
    g_vadOpen = FALSE;
}

/* One 20 ms frame of microphone (or test tone) in g_capFrame. Audio thread. */
static void VoiceProcessFrame(void)
{
    int level = VoiceFrameLevel(g_capFrame);
    InterlockedExchange(&g_micLevel100, level);

    /* voice-activation gate */
    int thr = g_vadThreshold, hangFrames = (g_vadHangMs + VOICE_FRAME_MS - 1) / VOICE_FRAME_MS;
    if (g_vadArmed) {
        g_vadAbove = level >= thr ? g_vadAbove + 1 : 0;
        if (!g_vadOpen && (g_vadAbove >= 2 || level >= thr + 15)) g_vadOpen = TRUE;
        if (g_vadOpen) {
            if (level >= thr - 8) g_vadHangLeft = hangFrames;
            else if (--g_vadHangLeft <= 0) g_vadOpen = FALSE;
        }
    } else {
        g_vadOpen = FALSE;
        g_vadAbove = g_vadHangLeft = 0;
    }

    BOOL send = g_txForce || g_vadOpen;
    if (send) {
        if (!g_txActive) {
            /* new talk spurt: fresh encoder state, then the pre-roll */
            if (g_enc) opus_encoder_ctl(g_enc, OPUS_RESET_STATE);
            int first = (g_prerollHead - g_prerollCount + VOICE_PREROLL) % VOICE_PREROLL;
            for (int i = 0; i < g_prerollCount; i++) VoiceEncodePcm(g_preroll[(first + i) % VOICE_PREROLL], FALSE);
            g_prerollCount = 0;
            InterlockedExchange(&g_txActive, 1);
#if CODOL_DIAGNOSTICS
            InterlockedIncrement(&g_statSpurts);
#endif
            if (VoiceDvarInt(g_dvDebug, 0)) Log("voice: transmit on (level %d, threshold %d, %s)", level, thr, g_txForce ? "key" : "voice activation");
        }
        VoiceEncodePcm(g_capFrame, FALSE);
        return;
    }
    if (g_txActive) {
        /* end of the spurt: this frame goes out as the last one */
        VoiceEncodePcm(g_capFrame, TRUE);
        InterlockedExchange(&g_txActive, 0);
        if (VoiceDvarInt(g_dvDebug, 0)) Log("voice: transmit off (level %d)", level);
        return;
    }
    memcpy(g_preroll[g_prerollHead], g_capFrame, sizeof(g_capFrame));
    g_prerollHead = (g_prerollHead + 1) % VOICE_PREROLL;
    if (g_prerollCount < VOICE_PREROLL) g_prerollCount++;
}

/* The stream is closing: a spurt still going out ends with the partial frame, marked last. */
static void VoiceCaptureEnd(void)
{
    if (g_txActive) {
        for (int i = g_capFill; i < VOICE_FRAME; i++) g_capFrame[i] = 0;
        VoiceEncodePcm(g_capFrame, TRUE);
        InterlockedExchange(&g_txActive, 0);
    }
    g_capFill = 0;
    VoiceGateReset();
    InterlockedExchange(&g_micLevel, 0);
    InterlockedExchange(&g_micLevel100, 0);
}

/* k33_voiceTone 1: a 440 Hz test tone instead of the microphone (no capture device); 2: the tone in
 * bursts, 600 ms on and 900 ms off, to exercise the voice-activation gate. */
static BOOL VoiceToneTick(BOOL want)
{
    static BOOL active;
    static DWORD start;
    static LONG64 made;
    static double phase;
    int tone = g_dvTone ? VoiceDvarInt(g_dvTone, 0) : 0;
    if (!tone) {
        if (active && !want) { active = FALSE; VoiceCaptureEnd(); }
        if (!active) return FALSE;
    }
    if (want && !active) { active = TRUE; start = GetTickCount(); made = 0; g_capFill = 0; VoiceGateReset(); }
    if (!active) return TRUE;
    LONG64 due = (LONG64)(GetTickCount() - start) * VOICE_RATE / 1000;
    while (made + VOICE_FRAME <= due) {
        BOOL on = tone != 2 || (made * 1000 / VOICE_RATE) % 1500 < 600;
        for (int i = 0; i < VOICE_FRAME; i++) {
            g_capFrame[i] = on ? 0.25f * (float)sin(phase) : 0.0f;
            phase += 2.0 * 3.14159265358979 * 440.0 / VOICE_RATE;
            if (phase > 6.283185307179586) phase -= 6.283185307179586;
        }
        VoiceProcessFrame();
        made += VOICE_FRAME;
    }
    if (!want) {
        active = FALSE;
        VoiceCaptureEnd();
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
        VoiceGateReset();
    }
    if (!d->running) return;

    /* drain everything the device has */
    double step = (double)d->rate / VOICE_RATE;
    for (;;) {
        UINT32 packet = 0;
        HRESULT hr = IAudioCaptureClient_GetNextPacketSize(d->cap, &packet);
        if (FAILED(hr)) {
            Log("voice: capture lost (0x%08lX), reopening", (unsigned long)hr);
            VoiceCaptureEnd();
            VoiceDevRelease(d);
            d->retryAt = GetTickCount() + 2000;
            return;
        }
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
                if (g_capFill == VOICE_FRAME) { VoiceProcessFrame(); g_capFill = 0; }
                d->pos += step;
            }
            d->pos -= 1.0;
            d->prev = x;
        }
        IAudioCaptureClient_ReleaseBuffer(d->cap, frames);
    }

    if (!want) {
        VoiceCaptureEnd();
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
#if CODOL_DIAGNOSTICS
        if (voices) InterlockedIncrement(&g_statPlayed);
#endif

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
        static LONG seenWantGen;
        if (InterlockedExchange(&g_devDirty, 0)) {
            VoiceListInputs();
            if (g_capFallback && g_cap.client) {   /* the chosen device may be back */
                VoiceCaptureEnd();
                VoiceDevRelease(&g_cap);
            }
        }
        if (g_inWantGen != seenWantGen) {          /* a new choice: reopen on it */
            seenWantGen = g_inWantGen;
            if (g_cap.client) {
                VoiceCaptureEnd();
                VoiceDevRelease(&g_cap);
                g_cap.retryAt = 0;
            }
            InterlockedExchange(&g_captureState, 0);
        }
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
    const BYTE *(*reg)(const char *, int, int, int, unsigned, const char *) = C1RegisterInt;
    g_voiceDvarsDone = TRUE;
    g_dvVoice = reg("codol_voice", 1, 0, 1, 0x1, "Voice chat: 1 on, 0 off (+set codol_voice 0 on the command line installs nothing)");
    g_dvVolume = reg("k33_voiceVolume", 100, 0, 400, 0x1, "Voice chat playback volume in percent");
    g_dvBitrate = reg("k33_voiceBitrate", 24000, 6000, 64000, 0x1, "Opus voice bitrate (bits per second)");
#if CODOL_DIAGNOSTICS
    g_dvLoopback = reg("k33_voiceLoopback", 0, 0, 1, 0, "Play your own voice back locally instead of sending it");
    g_dvDebug = reg("k33_voiceDebug", 0, 0, 2, 0, "Voice chat diagnostics in the console");
    g_dvTone = reg("k33_voiceTone", 0, 0, 2, 0, "Transmit a 440 Hz test tone instead of the microphone (2: in bursts)");
#endif
    g_dvMode = reg("k33_voiceMode", 0, 0, 1, 0x1, "Voice chat transmit mode: 0 push-to-talk, 1 voice activation (open mic gated by level)");
    g_dvThreshold = reg("k33_voiceThreshold", 30, 0, 100, 0x1,
                        "Voice activation level, 0..100 = -60..0 dBFS (RMS); k33voice shows the live mic level");
    g_dvHang = reg("k33_voiceHangMs", 400, 0, 3000, 0x1, "Voice activation: keep transmitting this long after the level drops");
    g_dvMicMute = reg("k33_voiceMicMute", 0, 0, 1, 0x1, "Microphone off (Options > Audio > Microphone)");
    g_dvLevel = reg("k33_voiceLevel", 0, -1, 100, 0, "Live microphone level 0..100 (read only; -1 no microphone)");
    g_dvMeter = reg("k33_voiceMeter", 0, 0, 1, 0, "Set by Options > Audio while shown: open the mic to meter it (nothing is sent)");
    {
        typedef const BYTE *(__cdecl *RegStr)(const char *, const char *, unsigned, const char *);
        RegStr regs = (RegStr)(DWORD_PTR)0x00624740u;
        g_dvInDevice = regs("k33_voiceInputDevice", "", 0x1, "Microphone: Windows endpoint ID (empty = the default communications device)");
        g_dvInList = regs("k33_voiceInputList", "", 0, "Microphones for Options > Audio: Default;name;... (read only)");
    }
    g_dvInPick = reg("k33_voiceInputPick", -1, -1, 64, 0, "Set by Options > Audio: choose the microphone at this position of k33_voiceInputList");
    g_dvInIndex = reg("k33_voiceInputIndex", 0, 0, 64, 0, "The chosen microphone's position in k33_voiceInputList (read only)");

    /* C1 does not apply "+set" from the command line to a dvar registered this late (and runs no
     * "+command" at all), so re-issue ours: "+set k33_voiceLoopback 1", "+toggletalk". */
    static const char *const kMine[] = { "codol_voice", "k33_voiceVolume", "k33_voiceBitrate",
                                         "k33_voiceLoopback", "k33_voiceDebug", "k33_voiceTone",
                                         "k33_voiceMode", "k33_voiceThreshold", "k33_voiceHangMs",
                                         "k33_voiceMicMute" };
    const char *cl = GetCommandLineA();
    for (unsigned i = 0; cl && i < sizeof(kMine) / sizeof(kMine[0]); i++) {
        char pat[64], cmd[96];
        _snprintf(pat, sizeof(pat), "+set %s ", kMine[i]);
        const char *at = strstr(cl, pat);
        if (!at) continue;
        at += strlen(pat);
        int v = atoi(at[0] == '"' ? at + 1 : at);
        _snprintf(cmd, sizeof(cmd), "set %s %d\n", kMine[i], v);
        C1CbufAddText(cmd);
        Log("voice: command line: set %s %d", kMine[i], v);
    }
    if (cl && strstr(cl, "+toggletalk")) InterlockedExchange(&g_toggleTalk, 1);
}

static BOOL VoiceEnabled(void) { return VoiceDvarInt(g_dvVoice, 1) != 0 && !g_uiVoiceOff; }

static BOOL VoiceMicMuted(void) { return VoiceDvarInt(g_dvMicMute, 0) != 0 || g_micMuted; }

/* A read-only dvar's value (k33_voiceLevel) or a consumed request (k33_voiceMeter) written in place:
 * the current integer at dvar+0xC, as C1DvarInt reads it. Main thread. */
static void VoiceDvarPoke(const BYTE *d, int v)
{
    if (d && *(volatile const int *)(d + 0xC) != v) *(volatile int *)(d + 0xC) = v;
}

/* The game window (any top-level window of this process) is in the foreground. */
static BOOL VoiceGameFocused(void)
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (!fg) return FALSE;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static DWORD g_meterUntil;

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
#if CODOL_DIAGNOSTICS
            InterlockedIncrement(&g_statSent);
#endif
        }
    }
}


/* ---- push-to-talk as a C1 bind ----------------------------------------------------------------------
 *
 * C1 binds are not command strings: playerKeys[lc].keys[k].binding (0x16F5164 + lc*0xD28 + k*12) is an
 * index into the name table 0xCD62C0 (86 entries; Key_GetBindingForCmd 0x492DB0 scans it, count
 * 0x56 at 0x492E22). CL_KeyEvent runs a binding through 0x491580(eax = localClient, index, key), a
 * switch over indices 1..85; indices below 0x47 come in +/- pairs (odd = +, the next = -), and the
 * key-up path (0x4934E5) and the clear-all-keys path (0x49383C) call index+1 for an odd index
 * below 0x47. "+talk" is not in the table, so "bind v +talk" stored 0 and the Options > Controls
 * > Chat rows (codo.gfx OptionView_control_chat: chatmodeteam, toggletalk, +talk, read through
 * AS_GetKeyBindingNamesFromCommand / AS_SetKeyBindingForCommand) could not bind it, and
 * "toggletalk" (79) is an empty case.
 *
 * The table is copied into one with two more entries, 87 "+talk" and 88 "-talk" (86 is a placeholder
 * so that +talk is odd like every other + command); its four users and the count point at the copy.
 * The two pair tests also accept 87, and the executor hook runs 87, 88 and 79 (toggletalk) here.
 * So +talk is bound, saved to config_mp.cfg by Key_WriteBindings ("bind V "+talk"") and rebound in
 * the Controls menu exactly like +melee, and C1's key routing (menus, chat line) decides when it
 * fires. */
#define VOICE_BIND_TABLE_VA   0x00CD62C0u
#define VOICE_BIND_COUNT      86
#define VOICE_BIND_TALK       87
#define VOICE_BIND_UNTALK     88
#define VOICE_BIND_TOGGLETALK 79
#define VOICE_KEYS_VA         0x016F5164u
#define VOICE_EXEC_VA         0x00491580u
#define VOICE_EXEC_RESUME     0x0049158Bu

static const char *g_bindNames[VOICE_BIND_UNTALK + 1];
static BOOL g_bindOk;
static int g_talkKeys[2];

/* Keys (C1 names, upper case like its own lists) bound to +talk for local client 0. */
static void VoiceTalkKeys(char *out, int n)
{
    int o = 0;
    out[0] = 0;
    if (!g_bindOk) return;
    for (int k = 1; k < 256 && o < n - 16; k++) {
        int b = *(volatile const int *)(DWORD_PTR)(VOICE_KEYS_VA + k * 12);
        if (b != VOICE_BIND_TALK) continue;
        char name[16];
        if (k > 32 && k < 127) _snprintf(name, sizeof(name), "%c", (char)toupper(k));
        else _snprintf(name, sizeof(name), "key %d", k);
        o += _snprintf(out + o, n - o, "%s%s", o ? ", " : "", name);
    }
    out[n - 1] = 0;
}

/* The default bind, as a default: V -> +talk only when no key is bound to +talk and V is free. */
static void VoiceDefaultBind(void)
{
    if (!g_bindOk || !g_c1CbufOk) return;
    for (int k = 0; k < 256; k++)
        if (*(volatile const int *)(DWORD_PTR)(VOICE_KEYS_VA + k * 12) == VOICE_BIND_TALK) return;
    int v = *(volatile const int *)(DWORD_PTR)(VOICE_KEYS_VA + 'v' * 12);
    if (v) {
        Log("voice: V is bound to %s and nothing is bound to +talk; no default bind",
            v > 0 && v <= VOICE_BIND_UNTALK && g_bindNames[v] ? g_bindNames[v] : "?");
        return;
    }
    C1CbufAddText("bind v +talk\n");
    Log("voice: default bind v +talk");
}

/* 0x491580's first stack argument is the binding index, the second the key. 1 = handled. */
__attribute__((used)) int __cdecl VoiceExecBinding(int index, int key)
{
    if (index == VOICE_BIND_TALK) { if (g_voiceOn) VoiceTalkDown(key); return 1; }
    if (index == VOICE_BIND_UNTALK) { if (g_voiceOn) VoiceTalkUp(key); return 1; }
    if (index == VOICE_BIND_TOGGLETALK) {
        if (g_voiceOn) {
            LONG v = !g_toggleTalk;
            InterlockedExchange(&g_toggleTalk, v);
            C1Printf("voice: toggletalk %s\n", v ? "on (transmitting)" : "off");
        }
        return 1;
    }
    return 0;
}

__attribute__((used)) DWORD g_voiceExecResume = VOICE_EXEC_RESUME;
void VOICE_ExecHook(void);
void VOICE_PairEdx(void);
void VOICE_PairEbx(void);
__asm__(
    ".text\n"
    ".globl _VOICE_ExecHook\n"
    "_VOICE_ExecHook:\n"                  /* jmp from 0x491580 */
    "    pushl %eax\n"
    "    pushl %ecx\n"
    "    pushl %edx\n"
    "    pushl 20(%esp)\n"                /* key */
    "    pushl 20(%esp)\n"                /* index */
    "    call _VoiceExecBinding\n"
    "    addl $8, %esp\n"
    "    testl %eax, %eax\n"
    "    popl %edx\n"
    "    popl %ecx\n"
    "    popl %eax\n"
    "    jz 1f\n"
    "    ret\n"
    "1:  pushl %ebp\n"                    /* the displaced first 11 bytes */
    "    movl %esp, %ebp\n"
    "    pushl %ecx\n"
    "    cmpb $0, 0x16FC2DA\n"
    "    jmp *_g_voiceExecResume\n"
    /* "is this binding a + command with a - partner": ZF clear when yes */
    ".globl _VOICE_PairEdx\n"
    "_VOICE_PairEdx:\n"
    "    cmpl $87, %edx\n"
    "    je 2f\n"
    "    cmpl $0x47, %edx\n"
    "    jge 3f\n"
    "    testb $1, %dl\n"
    "    ret\n"
    ".globl _VOICE_PairEbx\n"
    "_VOICE_PairEbx:\n"
    "    cmpl $87, %ebx\n"
    "    je 2f\n"
    "    cmpl $0x47, %ebx\n"
    "    jge 3f\n"
    "    testb $1, %bl\n"
    "    ret\n"
    "2:  pushl %eax\n"                    /* ZF = 0 */
    "    xorl %eax, %eax\n"
    "    incl %eax\n"
    "    popl %eax\n"
    "    ret\n"
    "3:  cmpl %eax, %eax\n"               /* ZF = 1 */
    "    ret\n");

static BOOL VoicePatchPair(DWORD site, const BYTE *expect, void *thunk)
{
    BYTE *p = (BYTE *)(DWORD_PTR)site;
    if (!Readable(p, 10) || memcmp(p, expect, 10)) return FALSE;
    BYTE code[10] = { 0xE8, 0, 0, 0, 0, 0x74, 0, 0x90, 0x90, 0x90 };
    LONG rel = (LONG)((DWORD_PTR)thunk - (site + 5));
    memcpy(code + 1, &rel, 4);
    /* the original je rel8 at +8 targets site + 10 + rel8; the new jz sits at +5 */
    code[6] = (BYTE)(expect[9] + 3);
    WriteCode(p, code, 10);
    return TRUE;
}

static void VoiceInstallBind(void)
{
    static const BYTE kExec[] = { 0x55, 0x8B, 0xEC, 0x51, 0x80, 0x3D, 0xDA, 0xC2, 0x6F, 0x01, 0x00 };
    static const BYTE kPairEdx[] = { 0x83, 0xFA, 0x47, 0x7D, 0x5E, 0xF6, 0xC2, 0x01, 0x74, 0x59 };
    static const BYTE kPairEbx[] = { 0x83, 0xFB, 0x47, 0x7D, 0x54, 0xF6, 0xC3, 0x01, 0x74, 0x4F };
    static const BYTE kCount[] = { 0x83, 0xF8, 0x56 };
    static const DWORD kTableRefs[] = { 0x00492C0Au, 0x00492C11u, 0x00492DC3u, 0x004930ACu };
    const DWORD *table = (const DWORD *)(DWORD_PTR)VOICE_BIND_TABLE_VA;
    BOOL ok = Readable((void *)(DWORD_PTR)VOICE_EXEC_VA, sizeof(kExec)) &&
              !memcmp((void *)(DWORD_PTR)VOICE_EXEC_VA, kExec, sizeof(kExec)) &&
              !memcmp((void *)(DWORD_PTR)0x004934E5u, kPairEdx, 10) && !memcmp((void *)(DWORD_PTR)0x0049383Cu, kPairEbx, 10) &&
              !memcmp((void *)(DWORD_PTR)0x00492E22u, kCount, 3) && Readable(table, VOICE_BIND_COUNT * 4) &&
              table[VOICE_BIND_COUNT - 1] && !strcmp((const char *)(DWORD_PTR)table[VOICE_BIND_COUNT - 1], "leaveads") &&
              !strcmp((const char *)(DWORD_PTR)table[VOICE_BIND_TOGGLETALK], "toggletalk");
    for (unsigned i = 0; ok && i < sizeof(kTableRefs) / sizeof(kTableRefs[0]); i++)
        ok = *(const DWORD *)(DWORD_PTR)kTableRefs[i] == VOICE_BIND_TABLE_VA;
    if (!ok) {
        Log("voice: key binding table bytes differ, +talk is not bindable (console +talk still works)");
        return;
    }
    for (int i = 0; i < VOICE_BIND_COUNT; i++) g_bindNames[i] = (const char *)(DWORD_PTR)table[i];
    g_bindNames[86] = "k33_reserved";
    g_bindNames[VOICE_BIND_TALK] = "+talk";
    g_bindNames[VOICE_BIND_UNTALK] = "-talk";
    DWORD nt = (DWORD)(DWORD_PTR)g_bindNames;
    for (unsigned i = 0; i < sizeof(kTableRefs) / sizeof(kTableRefs[0]); i++) WriteCode((void *)(DWORD_PTR)kTableRefs[i], &nt, 4);
    BYTE count = VOICE_BIND_UNTALK + 1;
    WriteCode((void *)(DWORD_PTR)0x00492E24u, &count, 1);
    VoicePatchPair(0x004934E5u, kPairEdx, (void *)VOICE_PairEdx);
    VoicePatchPair(0x0049383Cu, kPairEbx, (void *)VOICE_PairEbx);
    BYTE jmp[5] = { 0xE9 };
    LONG rel = (LONG)((DWORD_PTR)VOICE_ExecHook - (VOICE_EXEC_VA + 5));
    memcpy(jmp + 1, &rel, 4);
    WriteCode((void *)(DWORD_PTR)VOICE_EXEC_VA, jmp, 5);
    g_bindOk = TRUE;
    Log("voice: key bindings: +talk (87) / -talk (88) added to C1's bind table, toggletalk (79) runs; "
        "bindable in Options > Controls > Chat and with bind");
}

static void VoiceAnnounce(void)
{
    char keys[128];
    g_voiceAnnounced = TRUE;
    VoiceTalkKeys(keys, sizeof(keys));
    C1Printf("^3voice: Opus voice chat %s, %s. Push-to-talk: %s (+talk; rebind under Options > Controls > Chat or "
              "\"bind <key> +talk\"); toggletalk latches. Voice mode, microphone and activation level: Options > "
              "Audio. k33voice shows status.\n",
              VoiceEnabled() ? "on" : "OFF (codol_voice 0)",
              VoiceDvarInt(g_dvMode, 0) == 1 ? "voice activation" : "push-to-talk", keys[0] ? keys : "no key bound");
}

/* Test automation, only with "+set k33_cmdFile 1" on the command line: every half second the lines
 * of <exe dir>\k33_cmd_<pid>.txt are executed (Cbuf) and the file is deleted. */
static void VoiceCmdFile(void)
{
#if CODOL_LOCAL_TESTING
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
    C1CbufAddText(buf);
#endif
}

/* Once per Com_EventLoop pass (main thread). */
static const char *VoiceDvarStr(const BYTE *d)
{
    const char *v = d && Readable(d, 0x10) ? *(const char *const *)(d + 0xC) : NULL;
    return v && Readable(v, 1) ? v : "";
}

/* Main thread: k33_voiceInputDevice -> the audio thread; the device list and the chosen index for
 * Options > Audio; Options > Audio's pick -> k33_voiceInputDevice. */
static void VoiceInputTick(void)
{
    static LONG listGen = -1;
    static char lastWant[160] = "\x01";
    if (!g_dvInDevice || !g_c1CbufOk) return;
    const char *want = VoiceDvarStr(g_dvInDevice);
    if (strcmp(want, lastWant)) {
        lstrcpynA(lastWant, want, sizeof(lastWant));
        EnterCriticalSection(&g_voiceLock);
        lstrcpynA(g_inWant, want, sizeof(g_inWant));
        LeaveCriticalSection(&g_voiceLock);
        InterlockedIncrement(&g_inWantGen);
        Log("voice: microphone: %s", want[0] ? want : "Windows default");
        listGen = -1;
    }
    int pick = VoiceDvarInt(g_dvInPick, -1);
    if (pick >= 0) {
        char cmd[260];
        EnterCriticalSection(&g_voiceLock);
        const char *id = pick > 0 && pick <= g_inCount ? g_inIds[pick - 1] : "";
        _snprintf(cmd, sizeof(cmd), "set k33_voiceInputDevice \"%s\"; set k33_voiceInputPick -1\n", id);
        LeaveCriticalSection(&g_voiceLock);
        C1CbufAddText(cmd);
        VoiceDvarPoke(g_dvInPick, -1);
    }
    if (g_inListGen != listGen) {
        char list[1600], cmd[1700];
        int n = _snprintf(list, sizeof(list), "Default"), index = 0;
        listGen = g_inListGen;
        EnterCriticalSection(&g_voiceLock);
        for (int i = 0; i < g_inCount && n < (int)sizeof(list) - 100; i++) {
            n += _snprintf(list + n, sizeof(list) - n, ";%s", g_inNames[i]);
            if (want[0] && !strcmp(g_inIds[i], want)) index = i + 1;
        }
        LeaveCriticalSection(&g_voiceLock);
        _snprintf(cmd, sizeof(cmd), "set k33_voiceInputList \"%s\"\n", list);
        C1CbufAddText(cmd);
        VoiceDvarPoke(g_dvInIndex, index);
    }
}

__attribute__((used)) void __cdecl VoiceMainTick(void)
{
    static LONG ticks;
    if (++ticks == 1) Log("voice: first main-thread tick");
    BorderlessTick();
    if (!g_voiceDvarsDone) {
        if (!C1DvarsReady()) return;
        VoiceRegisterDvars();
        Log("voice: dvars registered at tick %ld (codol_voice %d, loopback %d)", ticks,
            VoiceDvarInt(g_dvVoice, -1), VoiceDvarInt(g_dvLoopback, -1));
    }
    {
        static DWORD started;
        static BOOL defaulted;
        if (!started) started = GetTickCount();
        if (!defaulted && GetTickCount() - started > 8000) { defaulted = TRUE; VoiceDefaultBind(); }
    }
    VoiceInputTick();
    const BYTE *adr = VoiceServerAdr();
    BOOL on = VoiceEnabled();
    InterlockedExchange(&g_connected, adr != NULL);
    if (adr && !g_voiceAnnounced) VoiceAnnounce();

    /* What the microphone is for this pass:
     *   key    push-to-talk held / toggletalk latched: transmit (also the override in voice activation)
     *   vad    voice activation (k33_voiceMode 1): the mic stays open in a match while the game window
     *          has focus, and the level gate decides (scoreboard, chat line and console do not stop it)
     *   meter  Options > Audio is showing its Input Level: open the mic, send nothing */
    BOOL muted = VoiceMicMuted();
    BOOL target = adr || VoiceDvarInt(g_dvLoopback, 0);
    BOOL key = on && !muted && (g_keyTalk || g_cmdTalk || g_toggleTalk) && target;
    BOOL vad = on && !muted && target && VoiceDvarInt(g_dvMode, 0) == 1 &&
               (VoiceGameFocused() || VoiceDvarInt(g_dvTone, 0)); /* the test tone is not the mic */
    if (g_dvMeter && VoiceDvarInt(g_dvMeter, 0)) {
        g_meterUntil = GetTickCount() + 1500; /* the options screen re-sets it every 10 frames */
        VoiceDvarPoke(g_dvMeter, 0);
    }
    BOOL meter = on && !muted && (LONG)(g_meterUntil - GetTickCount()) > 0;
    int thr = VoiceDvarInt(g_dvThreshold, 30), hang = VoiceDvarInt(g_dvHang, 400);
    if (thr != g_vadThreshold) InterlockedExchange(&g_vadThreshold, thr);
    if (hang != g_vadHangMs) InterlockedExchange(&g_vadHangMs, hang);
    if (vad != (g_vadArmed != 0)) {
        InterlockedExchange(&g_vadArmed, vad);
#if CODOL_DIAGNOSTICS
        if (VoiceDvarInt(g_dvDebug, 0) || g_lastTalkPrint < 3) {
            g_lastTalkPrint++;
            C1Printf("voice: voice activation %s\n", vad ? "armed (open mic)" : "off");
        }
#endif
    }
    if (key != (g_txForce != 0)) {
        InterlockedExchange(&g_txForce, key);
#if CODOL_DIAGNOSTICS
        if (VoiceDvarInt(g_dvDebug, 0) || g_lastTalkPrint < 3) {
            g_lastTalkPrint++;
            C1Printf("voice: %s\n", key ? "talking" : "stopped talking");
        }
#endif
    }
    BOOL talk = key || vad;
    BOOL open = talk || meter;
    if (open != (g_wantCapture != 0)) InterlockedExchange(&g_wantCapture, open);
    VoiceDvarPoke(g_dvLevel, g_captureState < 0 && open ? -1 : open ? (int)g_micLevel100 : 0);
    if (g_captureState < 0 && talk) {
        static DWORD warned;
        if (!warned || GetTickCount() - warned > 30000) {
            warned = GetTickCount();
            C1Printf("^1voice: microphone unavailable\n");
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
#if CODOL_DIAGNOSTICS
    InterlockedIncrement(&g_statRecv);
#endif
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
        C1Printf("voice: rx talker %d seq %u frames %d flags %d\n", talker, seq, count, flags);
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

/* IW's kbutton rules (IN_KeyDown/IN_KeyUp): up to two keys hold the button, a key-up with no key
 * number (typed "-talk") releases it, and talking lasts while any holding key is down. */
static void VoiceTalkDown(int key)
{
    if (key == g_talkKeys[0] || key == g_talkKeys[1]) return;
    if (!g_talkKeys[0]) g_talkKeys[0] = key;
    else if (!g_talkKeys[1]) g_talkKeys[1] = key;
    InterlockedExchange(&g_keyTalk, 1);
}

static void VoiceTalkUp(int key)
{
    if (key <= 0) g_talkKeys[0] = g_talkKeys[1] = 0;
    else if (g_talkKeys[0] == key) g_talkKeys[0] = 0;
    else if (g_talkKeys[1] == key) g_talkKeys[1] = 0;
    else return;
    if (!g_talkKeys[0] && !g_talkKeys[1]) InterlockedExchange(&g_keyTalk, 0);
}

static void __cdecl VoiceCmdTalkDown(void) { VoiceTalkDown(VoiceArgc() > 1 ? atoi(VoiceArgv(1)) : -1); }
static void __cdecl VoiceCmdTalkUp(void) { VoiceTalkUp(VoiceArgc() > 1 ? atoi(VoiceArgv(1)) : -1); }
static void __cdecl VoiceCmdToggle(void)
{
    Log("voice: toggletalk command");
    LONG v = !g_toggleTalk;
    InterlockedExchange(&g_toggleTalk, v);
    C1Printf("voice: toggletalk %s\n", v ? "on (transmitting)" : "off");
}

static void __cdecl VoiceCmdMute(void)
{
    if (VoiceArgc() < 2) {
        C1Printf("usage: k33voicemute <client number>   (toggles; k33voice lists who is talking)\n");
        return;
    }
    int n = atoi(VoiceArgv(1));
    if (n < 0 || n >= VOICE_TALKERS) return;
    LONG m = !g_talkers[n].muted;
    InterlockedExchange(&g_talkers[n].muted, m);
    C1Printf("voice: client %d %s\n", n, m ? "muted" : "unmuted");
}

static void __cdecl VoiceCmdStatus(void)
{
    char kn[128];
    VoiceTalkKeys(kn, sizeof(kn));
    C1Printf("voice: %s, %s, mic %s%s; push-to-talk %s, +talk %s, toggletalk %s, loopback %d, volume %d%%, "
              "bitrate %d\n",
              VoiceEnabled() ? "on" : "OFF", g_connected ? "connected" : "not in a match",
              g_captureState > 0 ? g_captureName : g_captureState < 0 ? "FAILED" : "not opened yet",
              VoiceMicMuted() ? " (muted)" : "", kn[0] ? kn : "no key bound",
              g_bindOk ? "bindable" : g_voiceCmdsOk ? "console only" : "UNAVAILABLE", g_toggleTalk ? "on" : "off", VoiceDvarInt(g_dvLoopback, 0),
              VoiceDvarInt(g_dvVolume, 100), VoiceDvarInt(g_dvBitrate, 24000));
    {
        int mode = VoiceDvarInt(g_dvMode, 0), thr = VoiceDvarInt(g_dvThreshold, 30);
        LONG lv = g_wantCapture ? g_micLevel100 : 0;
        C1Printf("voice: mode %d (%s), threshold %d (%d dBFS), hang %d ms; mic %s, level %ld (%ld dBFS), %s; "
                  "talk spurts %ld\n",
                  mode, mode == 1 ? "voice activation, +talk overrides" : "push-to-talk", thr, thr * 60 / 100 - 60,
                  VoiceDvarInt(g_dvHang, 400), g_wantCapture ? "open" : "closed", lv, lv * 60 / 100 - 60,
                  g_txActive ? "TRANSMITTING" : g_vadArmed ? "gate closed" : "not transmitting", g_statSpurts);
    }
    C1Printf("voice: playback %s; frames captured %ld, packets sent %ld, received %ld, mixed %ld, concealed %ld\n",
              g_renderState > 0 ? g_renderName : g_renderState < 0 ? "FAILED" : "not opened yet", g_statCapFrames,
              g_statSent, g_statRecv, g_statPlayed, g_statLostFrames);
    DWORD now = GetTickCount();
    for (int i = 0; i < VOICE_TALKERS; i++)
        if (g_lastHeard[i] && now - g_lastHeard[i] < 60000)
            C1Printf("voice:   client %d last heard %lu ms ago%s%s\n", i, now - g_lastHeard[i],
                      now - g_lastHeard[i] < 500 ? " (talking)" : "", g_talkers[i].muted ? " [muted]" : "");
}

/* Test automation: "k33ready [0|1]" presses the room's Ready button the way the Scaleform
 * SetReadyState callback does (AS_SetReadyState::Call 0x6AEFF0 reads the bool at args[0]+8). Only
 * with "+set k33_cmdFile 1" on the command line. */
#if CODOL_LOCAL_TESTING
static void __cdecl VoiceCmdReady(void)
{
    static const BYTE kReady[] = { 0x55, 0x8B, 0xEC, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x84, 0x05, 0xA8, 0xB0, 0x2E, 0x08 };
    const char *cl = GetCommandLineA();
    if (!cl || !strstr(cl, "k33_cmdFile 1")) { C1Printf("k33ready: test builds only (+set k33_cmdFile 1)\n"); return; }
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

/* Test automation: "k33mouse <x> <y> [click|off]" points C1 at a client-area position and clicks there
 * without the window having focus, so a test client next to the player's game can drive the menus:
 * C1's GetCursorPos / GetForegroundWindow / GetActiveWindow import slots (0xB423A4, 0xB423F4,
 * 0xB42454) report that position and the game window, and the click is posted as WM_LBUTTON*.
 * Only with "+set k33_cmdFile 1" on the command line. */
static volatile LONG g_fakeMouse;
static POINT g_fakeCursor;
static FARPROC g_realCursor, g_realFg, g_realActive;
#define VOICE_MAIN_HWND_VA 0x02B68104u

static BOOL WINAPI VoiceFakeCursorPos(LPPOINT p)
{
    HWND h = *(HWND *)(DWORD_PTR)VOICE_MAIN_HWND_VA;
    if (g_fakeMouse && h && p) { *p = g_fakeCursor; ClientToScreen(h, p); return TRUE; }
    return ((BOOL(WINAPI *)(LPPOINT))g_realCursor)(p);
}
static HWND WINAPI VoiceFakeFg(void)
{
    HWND h = *(HWND *)(DWORD_PTR)VOICE_MAIN_HWND_VA;
    return g_fakeMouse && h ? h : ((HWND(WINAPI *)(void))g_realFg)();
}
static HWND WINAPI VoiceFakeActive(void)
{
    HWND h = *(HWND *)(DWORD_PTR)VOICE_MAIN_HWND_VA;
    return g_fakeMouse && h ? h : ((HWND(WINAPI *)(void))g_realActive)();
}

static void __cdecl VoiceCmdMouse(void)
{
    const char *cl = GetCommandLineA();
    if (!cl || !strstr(cl, "k33_cmdFile 1")) { C1Printf("k33mouse: test builds only (+set k33_cmdFile 1)\n"); return; }
    if (!g_realCursor) {
        g_realCursor = *(FARPROC *)(DWORD_PTR)0x00B423A4u;
        g_realFg = *(FARPROC *)(DWORD_PTR)0x00B423F4u;
        g_realActive = *(FARPROC *)(DWORD_PTR)0x00B42454u;
        FARPROC f1 = (FARPROC)VoiceFakeCursorPos, f2 = (FARPROC)VoiceFakeFg, f3 = (FARPROC)VoiceFakeActive;
        WriteCode((void *)(DWORD_PTR)0x00B423A4u, &f1, 4);
        WriteCode((void *)(DWORD_PTR)0x00B423F4u, &f2, 4);
        WriteCode((void *)(DWORD_PTR)0x00B42454u, &f3, 4);
    }
    if (VoiceArgc() >= 2 && !_stricmp(VoiceArgv(1), "off")) { InterlockedExchange(&g_fakeMouse, 0); return; }
    if (VoiceArgc() < 3) return;
    g_fakeCursor.x = atoi(VoiceArgv(1));
    g_fakeCursor.y = atoi(VoiceArgv(2));
    InterlockedExchange(&g_fakeMouse, 1);
    HWND h = *(HWND *)(DWORD_PTR)VOICE_MAIN_HWND_VA;
    LPARAM lp = MAKELPARAM(g_fakeCursor.x, g_fakeCursor.y);
    if (h) PostMessageA(h, WM_MOUSEMOVE, 0, lp);
    if (h && VoiceArgc() >= 4 && !_stricmp(VoiceArgv(3), "click")) {
        PostMessageA(h, WM_LBUTTONDOWN, MK_LBUTTON, lp);
        PostMessageA(h, WM_LBUTTONUP, 0, lp);
    }
    Log("voice: k33mouse %ld %ld %s", g_fakeCursor.x, g_fakeCursor.y, VoiceArgc() >= 4 ? VoiceArgv(3) : "");
}

#endif
static C1Cmd g_voiceCmds[] = {
#if CODOL_LOCAL_TESTING
    { NULL, "k33mouse", (void *)VoiceCmdMouse },
    { NULL, "k33ready", (void *)VoiceCmdReady },
#endif
    { NULL, "+talk", (void *)VoiceCmdTalkDown },   { NULL, "-talk", (void *)VoiceCmdTalkUp },
    { NULL, "toggletalk", (void *)VoiceCmdToggle },
#if CODOL_DIAGNOSTICS
    { NULL, "k33voice", (void *)VoiceCmdStatus },
#endif
    { NULL, "k33voicemute", (void *)VoiceCmdMute },
};

/* Link the nodes into C1's command list. Done before the game entry: the list head 0x24F6424 is
 * plain zero-initialised data that Cmd_AddCommand (inlined, e.g. 0x498BA9) only prepends to, so the
 * commands exist before the command line and config_mp.cfg run ("bind CAPSLOCK +talk" works). */
static void VoiceRegisterCommands(void)
{
    static BOOL done;
    if (done) return;
    done = TRUE;
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_voiceCmds) / sizeof(g_voiceCmds[0]); i++) {
        if (C1AddCommand(&g_voiceCmds[i])) added++;
        else Log("voice: command %s not added (exists, or no command list)", g_voiceCmds[i].name);
    }
    g_voiceCmdsOk = added > 0;
    Log("voice: %d commands linked into the command list", added);
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
    if (!has) return;
    char cmd[48];
    _snprintf(cmd, sizeof(cmd), "set k33_voiceMicMute %d\n", v ? 1 : 0);
    if (!g_voiceDvarsDone || !C1CbufAddText(cmd)) InterlockedExchange(&g_micMuted, v);
    else InterlockedExchange(&g_micMuted, 0);
}


/* ---- HUD: the hud menu's voice ownerdraws (main thread, inside C1's menu paint) -------------------
 *
 * C1's ui_mp/hud_720.txt (common_mp) still carries IW4's voice items: "talking" (ownerdraw 266, the
 * local player's transmit icon) and "talker1".."talker4" (ownerdraws 193..196, a mic icon and the
 * name of each player heard), with their rects, text scale and visibility expressions. C1 stripped
 * the code behind them: CG_OwnerDraw 0x459E10 switches on ownerDraw-5 through the byte table
 * 0x45A8D8, and 193..196 land on case 29 (0x45A7B3, return) while 266 falls to the default
 * (0x45A636), so nothing is drawn. The materials are still in the zones: voice_on (common_mp) and
 * talkingicon (code_post_gfx_mp).
 *
 * VoiceOwnerDrawHook takes CG_OwnerDraw's first six bytes (push ebp; mov ebp,esp; sub esp,18h) and
 * handles those five ownerdraws the way IW4's CG_DrawPlayerTalking / CG_DrawTalkerNum do, with C1's
 * own calls: Material_RegisterHandle 0xA78170, ScrPlace_GetView 0x4B52A0, UI_DrawHandlePic 0x5DD460
 * (edx = scrPlace, ecx = color) and UI_DrawText 0x5EC3E0. Everything else continues into C1.
 * CG_OwnerDraw's stack arguments (ebp-relative): +8 localClientNum, +20h..+2Ch rect x/y/w/h, +30h/+34h
 * horz/vert align, +38h/+3Ch text x/y, +40h ownerDraw, +44h font, +48h text scale, +4Ch color,
 * +54h text style.
 *
 * "talking" shows while this client transmits; its alpha follows the mic level (dim when silent,
 * full when the mic picks something up). talkerN is the Nth player heard in the last 500 ms
 * (g_voiceChatTalkingDuration), mutes excluded; the name comes from the player-name configstring
 * 1280 + clientNum the server publishes. */
#define VOICE_OWNERDRAW_VA     0x00459E10u
#define VOICE_OWNERDRAW_RESUME 0x00459E16u
#define VOICE_MAT_REGISTER_VA  0x00A78170u
#define VOICE_SCRPLACE_VA      0x004B52A0u
#define VOICE_DRAWPIC_VA       0x005DD460u
#define VOICE_DRAWTEXT_VA      0x005EC3E0u
#define VOICE_OD_TALKER1       193
#define VOICE_OD_TALKING       266
#define VOICE_CS_NAMES         1280
#define VOICE_CS_OFFSETS_VA    0x06D0D130u
#define VOICE_CS_DATA_VA       0x06D1370Cu

static void *g_matVoiceOn;
static BOOL g_voiceHudOk;

static void VoicePlayerName(int client, char *out, int n)
{
    out[0] = 0;
    if (client >= 0 && client < 20) {
        const DWORD *offs = (const DWORD *)(DWORD_PTR)(VOICE_CS_OFFSETS_VA + (VOICE_CS_NAMES + client) * 4);
        if (Readable(offs, 4) && *offs) {
            const char *str = (const char *)(DWORD_PTR)(VOICE_CS_DATA_VA + *offs);
            int k = 0;
            for (const char *c = str; k < n - 1 && Readable(c, 1) && *c; c++)
                if (*c != '"') out[k++] = *c;
            out[k] = 0;
        }
    }
    if (!out[0]) _snprintf(out, n, "Player %d", client);
    out[n - 1] = 0;
}

static int VoiceLocalClient(void)
{
    BYTE *cg = ParityCg();
    return cg && Readable(cg + PS_CLIENTNUM, 4) ? *(const int *)(cg + PS_CLIENTNUM) : -1;
}

/* The Nth (0-based) player heard in the last 500 ms, or -1. */
static int VoiceTalkerNum(int nth)
{
    DWORD now = GetTickCount();
    for (int i = 0; i < VOICE_TALKERS; i++) {
        DWORD t = g_lastHeard[i];
        if (!t || now - t > 500 || g_talkers[i].muted) continue;
        if (nth-- == 0) return i == VOICE_LOOPBACK_TALKER ? VoiceLocalClient() : i; /* k33_voiceLoopback: you */
    }
    return -1;
}

static void *VoiceMaterial(void)
{
    if (!g_matVoiceOn)
        g_matVoiceOn = ((void *(__cdecl *)(const char *, int))(DWORD_PTR)VOICE_MAT_REGISTER_VA)("voice_on", 7);
    return g_matVoiceOn;
}

/* UI_DrawHandlePic 0x5DD460 takes scrPlace in edx and the colour in ecx; the thunk moves them there
 * and re-pushes the other seven arguments (each "push [esp+36]" takes the next lower one). */
__attribute__((used)) DWORD g_voiceDrawPic = VOICE_DRAWPIC_VA;
void __cdecl VOICE_DrawPic(const void *scr, const float *color, float x, float y, float w, float h, int horz,
                           int vert, void *material);
__asm__(
    ".text\n"
    ".globl _VOICE_DrawPic\n"
    "_VOICE_DrawPic:\n"
    "    movl 4(%esp), %edx\n"
    "    movl 8(%esp), %ecx\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    call *_g_voiceDrawPic\n"
    "    addl $28, %esp\n"
    "    ret\n");

typedef void(__cdecl *UiDrawTextFn)(const void *, const char *, int, void *, float, float, int, int, float,
                                    const float *, int);

static int HudStyleOwnerDraw(const DWORD *args); /* hud_style.c: the HUD styles' ownerdraws share this hook */
/* args points at CG_OwnerDraw's first stack argument ([ebp+8]). 1 = handled here. */
__attribute__((used)) int __cdecl VoiceOwnerDraw(const DWORD *args)
{
    if (HudStyleOwnerDraw(args)) return 1;
    int ownerDraw = (int)args[(0x40 - 8) / 4];
    if (ownerDraw != VOICE_OD_TALKING && (ownerDraw < VOICE_OD_TALKER1 || ownerDraw > VOICE_OD_TALKER1 + 3)) return 0;
    if (!g_voiceOn || !g_voiceDvarsDone || !VoiceEnabled()) return 1;
    int lc = (int)args[0];
    float x = *(const float *)&args[(0x20 - 8) / 4], y = *(const float *)&args[(0x24 - 8) / 4];
    float w = *(const float *)&args[(0x28 - 8) / 4], h = *(const float *)&args[(0x2C - 8) / 4];
    int horz = (BYTE)args[(0x30 - 8) / 4], vert = (BYTE)args[(0x34 - 8) / 4];
    void *font = (void *)(DWORD_PTR)args[(0x44 - 8) / 4];
    float scale = *(const float *)&args[(0x48 - 8) / 4];
    const float *color = (const float *)(DWORD_PTR)args[(0x4C - 8) / 4];
    int style = (int)args[(0x54 - 8) / 4];
    float c[4] = { 1, 1, 1, 1 };
    if (color && Readable(color, 16)) memcpy(c, color, sizeof(c));
    void *mat = VoiceMaterial();
    if (!mat) return 1;
    const void *scr = ((const void *(__cdecl *)(int))(DWORD_PTR)VOICE_SCRPLACE_VA)(lc);

    if (ownerDraw == VOICE_OD_TALKING) {
        if (!g_txActive) return 1; /* transmitting: key held, or the voice-activation gate open */
        LONG lv = g_micLevel;
        float a = lv > 40 ? 1.0f : lv > 12 ? 0.75f : 0.4f; /* picking something up: full brightness */
        c[3] *= a;
        VOICE_DrawPic(scr, c, x, y, w, h, horz, vert, mat);
        return 1;
    }
    int client = VoiceTalkerNum(ownerDraw - VOICE_OD_TALKER1);
    if (client < 0) return 1;
    char name[64];
    VoicePlayerName(client, name, sizeof(name));
    VOICE_DrawPic(scr, c, x, y, w, h, horz, vert, mat);
    if (font)
        ((UiDrawTextFn)(DWORD_PTR)VOICE_DRAWTEXT_VA)(scr, name, 32, font, x + w + 2.0f, y + h - 1.0f, horz, vert,
                                                     scale, c, style);
    return 1;
}

/* ---- thunks and install --------------------------------------------------------------------------- */

__attribute__((used)) DWORD g_voiceEventLoop = VOICE_EVENTLOOP_FUNC_VA;
__attribute__((used)) DWORD g_voiceRecvDone = VOICE_RECV_DONE_VA;
__attribute__((used)) DWORD g_voiceOwnerDrawResume = VOICE_OWNERDRAW_RESUME;
void VOICE_TickHook(void);
void VOICE_RecvHook(void);
void VOICE_OwnerDrawHook(void);
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
    "    jmp *_g_voiceRecvDone\n"     /* mov al, 1 ... ret */
    ".globl _VOICE_OwnerDrawHook\n"
    "_VOICE_OwnerDrawHook:\n"         /* jmp from CG_OwnerDraw 0x459E10, before its prologue */
    "    leal 4(%esp), %eax\n"         /* the first stack argument ([ebp+8] in C1's frame) */
    "    pushl %eax\n"
    "    call _VoiceOwnerDraw\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jz 1f\n"
    "    ret\n"
    "1:  pushl %ebp\n"                 /* the displaced prologue */
    "    movl %esp, %ebp\n"
    "    subl $0x18, %esp\n"
    "    jmp *_g_voiceOwnerDrawResume\n");

static BOOL VoiceSlotHook(DWORD slot, void *fn)
{
    DWORD *p = (DWORD *)(DWORD_PTR)slot;
    if (!Readable(p, 4) || *p != VOICE_AS_NOP) return FALSE;
    DWORD v = (DWORD)(DWORD_PTR)fn;
    WriteCode(p, &v, 4);
    return TRUE;
}

static void InstallVoice(void)
{
    static const BYTE kRecv[] = { 0x68, 0x7C, 0x7E, 0xC4, 0x00, 0x6A, 0x10, 0xE8 };     /* push msg; push 10h; call */
    static const BYTE kRecvDone[] = { 0xB0, 0x01, 0x5F, 0x5E, 0x5B };                  /* mov al,1; pop edi/esi/ebx */
    static const BYTE kRecvMsg[] = { 0x89, 0x8D, 0x94, 0xF3, 0xFF, 0xFF };             /* mov [ebp-0C6Ch], ecx @0x496421 */
    static const BYTE kOob[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53 };
    static const BYTE kClc[] = { 0x8B, 0x3D, 0x68, 0x80, 0x6F, 0x01, 0x6A, 0x01, 0x81, 0xF7, 0xCC, 0x50, 0xF9, 0x26 };

    const char *cl = GetCommandLineA();
    if (cl && (strstr(cl, "codol_voice 0") || strstr(cl, "codol_voice \"0\""))) {
        Log("voice: off (codol_voice 0)");
        return;
    }
    InitializeCriticalSection(&g_voiceLock);
    C1GameInit();

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

    VoiceInstallBind();

    /* commands */
    VoiceRegisterCommands();

    /* hud menu voice ownerdraws (talking 266, talker1..4 193..196) */
    {
        static const BYTE kOd[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x8B, 0x15, 0xAC, 0xD2, 0xC3, 0x06 };
        static const BYTE kOdSwitch[] = { 0x81, 0xFA, 0x21, 0x01, 0x00, 0x00 }; /* cmp edx, 121h @0x459E68 */
        static const BYTE kPic[] = { 0x55, 0x8B, 0xEC, 0xD9, 0xEE, 0x83, 0xEC, 0x0C };
        static const BYTE kText[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xC0, 0x83, 0xEC, 0x34 };
        BYTE *od = (BYTE *)(DWORD_PTR)VOICE_OWNERDRAW_VA;
        if (Readable(od, sizeof(kOd)) && !memcmp(od, kOd, sizeof(kOd)) &&
            !memcmp((void *)(DWORD_PTR)0x00459E68u, kOdSwitch, sizeof(kOdSwitch)) &&
            !memcmp((void *)(DWORD_PTR)VOICE_DRAWPIC_VA, kPic, sizeof(kPic)) &&
            !memcmp((void *)(DWORD_PTR)VOICE_DRAWTEXT_VA, kText, sizeof(kText)) &&
            CallsTarget(0x0044EE08u, VOICE_MAT_REGISTER_VA)) {
            BYTE jmp[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
            LONG rel = (LONG)((DWORD_PTR)VOICE_OwnerDrawHook - (VOICE_OWNERDRAW_VA + 5));
            memcpy(jmp + 1, &rel, 4);
            WriteCode(od, jmp, 6);
            g_voiceHudOk = TRUE;
        }
    }

    BOOL asEnable = VoiceSlotHook(VOICE_AS_ENABLE_SLOT, (void *)VoiceAsEnable);
    BOOL asMute = VoiceSlotHook(VOICE_AS_MICMUTE_SLOT, (void *)VoiceAsMicMute);

    g_voiceOn = tickOk || g_voiceRecvOk;
    if (g_voiceOn) {
        HANDLE t = CreateThread(NULL, 0, VoiceThread, NULL, 0, NULL);
        if (t) { SetThreadPriority(t, THREAD_PRIORITY_ABOVE_NORMAL); CloseHandle(t); }
        else g_voiceOn = FALSE;
    }
    Log("voice: hud menu ownerdraws (talking 266, talker1-4 193-196) %s", g_voiceHudOk ? "hooked (CG_OwnerDraw 0x459E10)" : "NOT hooked (bytes differ)");
    Log("voice: send %s, tick %s, receive %s, push-to-talk bind %s, UI EnableVoiceTalk %s, SetVoiceMicMute %s, opus %s%s",
        g_voiceSendOk ? "ok" : "bytes differ", tickOk ? "hooked (0x595E9D)" : "NOT hooked",
        g_voiceRecvOk ? "hooked (0x496482)" : "NOT hooked (bytes differ)", g_bindOk ? "ok" : "NOT installed",
        asEnable ? "hooked" : "bytes differ", asMute ? "hooked" : "bytes differ", opus_get_version_string(),
        g_voiceOn ? "" : " -- voice OFF");
}
