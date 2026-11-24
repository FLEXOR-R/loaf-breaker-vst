/* LOAF BREAKER VST: the firmware's core as a library (lbcore.c), a C API the plugin loads (lbcore.dll / liblbcore.so).
 * Everything is called from one thread: the plugin's audio thread. */
#pragma once
#include <stdint.h>
#ifdef _WIN32
#define LB_API __declspec(dllexport)
#else
#define LB_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* the page as the FM-1's screen shows it, for the VST's own interface (lb_model): its domain, its tabs (the one lit),
 * the part ALGORITHM turns, the four knobs (their names and values, SEL's when SEL is held or locked), the black keys'
 * functions, the transport */
#define LB_TABS 16
typedef struct {
    char dom[16], label[16];             /* LIVE, SOUNDS...; KICK, DRUMS, BREAK or the domain */
    int ntabs, tab_on;
    char tabs[LB_TABS][16];
    char part[24];                       /* the part ALGORITHM turns ("" none) */
    char kname[4][16], kval[4][20];
    float knorm[4];                      /* 0..1, or -1: no gauge */
    int sel;                             /* SEL held or locked: the knobs are SEL + K1-K4's */
    char pad[12][8], padlong[12][20];    /* the black keys' functions (11), then the REC key's */
    int padon[12];
    int playing, step, bar16, slice, key, page, novice;
    float bpm, pos;                      /* the tempo; the playhead in the break's samples */
    char brk[16];
    int pattern;                         /* 1..64 */
    char say[2][24];                     /* the header's message (app_say), "" none */
    uint32_t say_n;                      /* moves with each new message */
} lb_model_t;
enum { LB_FX, LB_SEL, LB_ENV, LB_LFO, LB_EDIT, LB_GLO, LB_HOME, LB_SAVE, LB_ARP, LB_SEQ, LB_PLAY, LB_REC, LB_OCTDN, LB_OCTUP };
enum { LB_SELECT, LB_ALGO, LB_PRESET, LB_K1, LB_K2, LB_K3, LB_K4 };
#define LB_ABI 3
LB_API int lb_abi(void);
/* boot (with the saved flash image given before, or a new FM-1's), then run up to `ms` simulated milliseconds silently
 * (the start screen; the mode question answered EXPERT) */
LB_API void lb_init(uint32_t ms);
/* one simulated millisecond: input, the app's loop, then its frame (UI) every few ms */
LB_API void lb_ms(int frame);
/* `frames` stereo frames at 44.1 kHz, interleaved, -1..1 */
LB_API void lb_audio(float *lr, uint32_t frames);
LB_API int lb_midi_in(uint32_t pkt);      /* a USB-MIDI packet (cable 0) */
LB_API int lb_midi_out(uint32_t *pkt);
LB_API void lb_button(int b, int down);   /* LB_FX .. LB_OCTUP */
LB_API void lb_key(int k, int down, int vel);   /* 0..26: the FM-1's keys, F3 .. G5 */
LB_API void lb_turn(int role, int detents);     /* LB_SELECT .. LB_K4 */
LB_API void lb_master(int v);                   /* 0..1023 */
LB_API uint32_t lb_screen(uint16_t *rgb565);    /* the 240 x 240 screen copied; its change count */
LB_API uint32_t lb_leds(uint8_t *out, uint32_t n);   /* the LEDs (columns x lines), n bytes at most */
LB_API int lb_playing(void);
LB_API uint32_t lb_flash_size(void);
LB_API uint32_t lb_flash_get(uint8_t *out, uint32_t n);   /* the saved objects as the FM-1 keeps them */
LB_API int lb_flash_set(const uint8_t *in, uint32_t n);   /* before lb_init */
LB_API void lb_bank(const uint8_t *p, uint32_t n);   /* the break bank (breaks.bank), before lb_init; kept alive by the caller */
LB_API int lb_breaks(void);
LB_API int lb_get(int what);                     /* 0 break, 1 breaks, 2 per-break bytes saved, 3 length, 4 rate */                      /* how many breaks there are */
LB_API void lb_model(lb_model_t *m);
/* the break playing: its waveform in n columns (peak 0..1, then low / mid / high energy 0..1, 4 floats a column), its
 * slices' starts in 0..1 (up to 32), their count; its serial (moves when another break comes in) */
LB_API uint32_t lb_wave(float *cols, int n, float *slices, int *nslices);
LB_API int lb_key_index(int black, int i);       /* the FM-1 key (0..26) of white key i (0..15) or black key i (0..10) */
LB_API void lb_save(void);
/* the VST <-> FM-1 translation (fm1sync.c): the state as an FM-1 backup image (LBRK; room: 80 KB), its length or 0;
 * an FM-1 backup into the store (0 done, 1 not one, 2 damaged), then the core booted again from lb_flash_get */
LB_API uint32_t lb_fm1_export(uint8_t *out, uint32_t cap, char *report, uint32_t rcap);
LB_API int lb_fm1_import(const uint8_t *img, uint32_t n, char *report, uint32_t rcap);                      /* SAVE: everything into the flash image */
LB_API void lb_sync(int on);                    /* follow the MIDI clock in (tempo, start, stop) */
#ifdef __cplusplus
}
#endif
