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
    int skin;                            /* the FM-1's skin (SETUP K1), 0..23 */
    int kslot;                           /* the kick playing, 1..20, and the built-in it came from */
    char kpre[16];
    float klev[5];                       /* its layers' levels 0..1: SUB PUNCH GRIT CLICK, then TOK */
    char dpre[3][16];                    /* SN HH CP: their presets, their levels */
    float dlev[3];
} lb_model_t;
enum { LB_FX, LB_SEL, LB_ENV, LB_LFO, LB_EDIT, LB_GLO, LB_HOME, LB_SAVE, LB_ARP, LB_SEQ, LB_PLAY, LB_REC, LB_OCTDN, LB_OCTUP };
enum { LB_SELECT, LB_ALGO, LB_PRESET, LB_K1, LB_K2, LB_K3, LB_K4 };
#define LB_ABI 13
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
LB_API void lb_tempo(int detents);              /* the tempo, 1 BPM a detent (SELECT never in the VST) */
LB_API void lb_master(int v);                   /* 0..1023 */
LB_API uint32_t lb_screen(uint16_t *rgb565);    /* the 240 x 240 screen copied; its change count */
LB_API uint32_t lb_leds(uint8_t *out, uint32_t n);   /* the LEDs (columns x lines), n bytes at most */
/* the lights as the firmware gave them (plat_lights, plat_lights_mid): o[0] the buttons lit (bit 14: PLAY's green),
 * o[1] the buttons glowing, o[2] the keys lit, o[3] mid, o[4] glowing (bit k: key k, 0..26), o[5] the glow 0..2 (the
 * FM-1 as the VST's controller lights itself from them) */
LB_API void lb_lights(uint32_t *o);
LB_API int lb_playing(void);
LB_API uint32_t lb_flash_size(void);
LB_API uint32_t lb_flash_get(uint8_t *out, uint32_t n);   /* the saved objects as the FM-1 keeps them */
LB_API int lb_flash_set(const uint8_t *in, uint32_t n);   /* before lb_init */
LB_API void lb_bank(const uint8_t *p, uint32_t n);   /* the break bank (breaks.bank), before lb_init; kept alive by the caller */
LB_API int lb_breaks(void);
LB_API int lb_break_name(int i, char *out, int cap);   /* break i's name (1), 0 none */
LB_API void lb_break_go(int i);
/* the song: a track (0..7) read whole, written whole (each item checked), played / stopped / picked. it[]: 0 the end,
 * 1..64 that pattern, 0xFE a drop, 0xFD a gap; rep its loops (1..16); tb a pattern item's break (0 the one playing,
 * else break + 1; a drop's: bit 0 stutter, 1-2 gap keeps, 3-6 its DROP n); tk its kick (0 the one playing, else KICK n) */
typedef struct {
    int track, cur, on, pos, end, from;  /* this one; the track picked; playing; the item playing; at its end 0 LOOP
                                          * 1 STOP 2 HOLD; the item it starts from */
    uint8_t it[32], rep[32], tb[32], tk[32];
    int patbars, dropbars;               /* a pattern's loop in bars; a drop's build */
} lb_track_t;
LB_API void lb_track(int t, lb_track_t *o);
LB_API void lb_track_write(const lb_track_t *t);
LB_API void lb_track_play(int op, int t);                       /* break i, as ALGORITHM asks for one */
LB_API int lb_get(int what);                     /* 0 break, 1 breaks, 2 per-break bytes saved, 3 length, 4 rate */                      /* how many breaks there are */
LB_API void lb_model(lb_model_t *m);
/* the break playing: its waveform in n columns (peak 0..1, then low / mid / high energy 0..1, 4 floats a column), its
 * slices' starts in 0..1 (up to 32), their count; its serial (moves when another break comes in) */
LB_API uint32_t lb_wave(float *cols, int n, float *slices, int *nslices);
/* the sound's picture (FLEXOR: "un vrai visuel sur le kick, une vraie waveform"): what 0 the kick as heard, 1..3 SN HH CP;
 * n columns (up to 2048) over ms (20..3000): 5 floats a column, the wave (-1..1, the kick's), lows, mids, highs, peak
 * (0..1, the FM-1's RGB view); lb_pic_sig moves when the sound changes. The break: lb_wave_mm, its min / max a column */
LB_API uint32_t lb_pic_sig(int what);
LB_API int lb_pic(int what, int n, int ms, float *out);
LB_API int lb_wave_mm(float *mm, int n);
LB_API void lb_drum_hit(int d);
LB_API int lb_key_index(int black, int i);       /* the FM-1 key (0..26) of white key i (0..15) or black key i (0..10) */
LB_API void lb_save(void);
/* the sequencer (the VST's PATTERN page; FLEXOR, 24 Nov 2026: "il me faut un vrai séquenceur"): the pattern in RAM whole,
 * as the firmware's SEQ has it (the Studio's ED_SEQ, read straight) */
typedef struct {
    int pat, next, playing, bar64, bstep;  /* 0..63, the next (-1 none), PLAY, the drums' step 0..63 and the break's 0..31
                                          * (-1 none) */
    int slices, blen;                      /* the break's slices, its steps (32) */
    int bars[4];                           /* KICK SN HH CP: their bars (1, 2, 4) */
    int novice, changed, clip;             /* BEGINNER; the group in RAM changed (another group: SAVE first); the
                                          * pattern copied (-1 none) */
    uint16_t mask[4][4];                   /* each drum's bars, a bit a step */
    uint8_t sb[4][64];                     /* each step's HITS - 1 (bits 0-1), COND (2-4), NUDGE (5-7); the drums' 32 */
    int8_t note[64];                       /* the kick's notes, semitones */
    uint8_t brk[32][3];                    /* the break's steps: slice (0 OFF, 1 LOOP, 2.. slice 1..), pitch (int8), flags
                                          * (bit 0 reverse, 1-2 repeat - 1) */
    char snd[4][20];                       /* what each lane plays ("KICK 4", "SNARE 2" ...) */
    char brkname[16];
} lb_seq_t;
LB_API void lb_seq(lb_seq_t *q);
/* a sequencer edit, through SEQ's own code (the same clamps and undo): op 1 a drum step on / off (a the drum 0..3, b its
 * step 0..63), 2 a drum step's knob (c 0 NOTE, 1 HITS, 2 COND, 3 NUDGE; d the turn), 3 a lane's (b its bar; c 0 FILL,
 * 1 SOUND, 2 CLEAR / 4X4, 3 LENGTH), 4 a break step's (b 0..31; c 0 SLICE, 1 PITCH, 2 REPEAT, 3 REVERSE), 5 the pattern
 * a (0..63), 6 undo; 7 copy the pattern, 8 paste it here, 9 a lane a step later / earlier (a 0..3 the drums, 4 the
 * break; d), 10 a lane's bar copied (a; b the bar, c the bar it goes to), 11 the drums cleared, 12 the break's steps
 * cleared, 13 the whole pattern cleared; 1 done, 0 refused */
LB_API int lb_step(int op, int a, int b, int c, int d);
/* the 64 patterns' pictures (four groups of 16, the one in RAM as it is, the others as saved): each 40 bytes, the drums'
 * bars (4), their masks (4 x 4 u16), the break's steps on (u32) */
LB_API void lb_patterns(uint8_t *out /* 64 x 40 */);
/* the pages as menus (FLEXOR, 24 Nov 2026: "des vrais menus, pas juste quelque chose pour contrôler le petit aperçu"):
 * every part of the tab shown (ALGORITHM's round, from the one shown), each with its four knobs, looked at ahead and the
 * core put back as it was; their count (1: the tab has no parts) */
typedef struct {
    char part[24];
    char kname[4][16], kval[4][20];
    float knorm[4];
} lb_part_t;
LB_API int lb_scan(lb_part_t *out, int max);
/* every value knob k (0..3) of part p (lb_scan's order) can take, in order, the one it has now at cur; wrap: a list
 * that goes round; more: longer than this holds. Looked at ahead, the core put back */
#define LB_OPT_MAX 256
#define LB_OPT_LEN 20
typedef struct {
    int n, cur, wrap, more;
    char v[LB_OPT_MAX][LB_OPT_LEN];
} lb_opts_t;
LB_API int lb_options(int part, int knob, lb_opts_t *o);
/* an effect's page (a kick's FX1 / FX2, a slot of the chain): the effects PRESETS goes through, in order, and the one
 * in use (cur); 0 elsewhere. Picked: PRESETS turned by the difference */
LB_API int lb_fx_list(char (*names)[16], int max, int *cur);
/* part p's knob k (lb_scan's order) moved `steps` values (each a change, whatever detents it takes), the part shown
 * then back; the values it moved */
LB_API int lb_set(int part, int knob, int steps);
/* the VST <-> FM-1 translation (fm1sync.c): the state as an FM-1 backup image (LBRK; room: 80 KB), its length or 0;
 * an FM-1 backup into the store (0 done, 1 not one, 2 damaged), then the core booted again from lb_flash_get */
LB_API uint32_t lb_fm1_export(uint8_t *out, uint32_t cap, char *report, uint32_t rcap);
LB_API int lb_fm1_import(const uint8_t *img, uint32_t n, char *report, uint32_t rcap);                      /* SAVE: everything into the flash image */
LB_API void lb_sync(int on);
/* the pick (FLEXOR: "exporter ou importer des choses, patterns ou tracks ou kicks"): some kicks (0..19), patterns
 * (0..63), tracks (0..7) between the VST and an FM-1 backup, the rest of the other side as it was */
enum { LB_PICK_KICK, LB_PICK_PATTERN, LB_PICK_TRACK };
typedef struct { uint8_t kind, from, to, rsv; } lb_pick_t;
typedef struct {
    char kick[20][20];                   /* each kick memory's PRESET name */
    int kick_live;                       /* the kick playing (0..19) */
    uint8_t pat[64];                     /* 0 never saved, 1 empty, 2 notes on it */
    uint8_t trk[8];                      /* each track's items */
} lb_list_t;
LB_API int lb_fm1_list(const uint8_t *img, uint32_t n, lb_list_t *o);   /* img 0: the VST's; 0 done */
LB_API uint32_t lb_fm1_put(const uint8_t *fm1, uint32_t n, const lb_pick_t *p, int np, uint8_t *out, uint32_t cap,
                           char *report, uint32_t rcap);   /* VST -> the FM-1's backup: the new image's length */
LB_API int lb_fm1_take(const uint8_t *fm1, uint32_t n, const lb_pick_t *p, int np, char *report, uint32_t rcap);
                                         /* the FM-1's backup -> the VST's store (then a fresh core): the picks taken */
/* one kick whole as a blob (the plugin's library: each a file, named, in folders): its size; read from kick memory k of
 * an FM-1 backup (img) or the VST's (img 0); heard alone (stereo 44.1 kHz frames, the core as it was after); put into
 * an FM-1 backup's memories (the new image) or the VST's (its store, then a fresh core) */
LB_API uint32_t lb_kick_size(void);
LB_API int lb_kick_read(const uint8_t *img, uint32_t n, int k, uint8_t *blob);
LB_API uint32_t lb_kick_render(const uint8_t *blob, float *lr, uint32_t frames);
LB_API uint32_t lb_fm1_put_kicks(const uint8_t *fm1, uint32_t n, const uint8_t *blobs, const uint8_t *to, int nk,
                                 uint8_t *out, uint32_t cap);
LB_API int lb_vst_put_kicks(const uint8_t *blobs, const uint8_t *to, int nk);                    /* follow the MIDI clock in (tempo, start, stop) */
#ifdef __cplusplus
}
#endif
