/* LOAF BREAKER VST: the firmware's core (fm1-base's host/rfm1.c: the host simulator as a library, compiled natively
 * here, with the whole app: plat_host.c, app.c and its pages) and the C API of lbcore.h over its panel. */
#define _GNU_SOURCE 1
#include <stdint.h>
const uint8_t *lb_bank_ptr;              /* (LB_BIG: the break bank the plugin loads, breaks.bank; app.c reads it) */
uint32_t lb_bank_len;
#include "rfm1.c"
#include "lbcore.h"
#include "fm1sync.c"
#include "lbsnap.c"
void lb_bank(const uint8_t *p, uint32_t n) { lb_bank_ptr = n >= 16u ? p : 0; lb_bank_len = n; }
int lb_abi(void) { return LB_ABI; }
static uint32_t lb_now;
static int32_t lb_nav[3];                /* SELECT, ALGORITHM, PRESET: detents still to hand over, one at a time (the
                                          * pages move a tab a reading, whatever its count) */
void lb_ms(int frame)
{
    /* the first start's question (tutor.c TUT_ASK): EXPERT, white key 3, as a player would (the VST starts EXPERT) */
    if (set.tut == 0xFDu && !ui.lobo_t && !ui.intro && lb_now & 1u)   /* (a tap every other ms: a key going down) */
        rf_panel.keys_tap |= 1u << WK[2];
    for (int r = 0; r < 3; r++)          /* (the last one read by the app: the next) */
        if (lb_nav[r] && !enc_acc[r] && !rf_panel.enc[rf_panel.enc_id[r]]) {
            int32_t s = lb_nav[r] > 0 ? 1 : -1;
            lb_nav[r] -= s;
            rf_panel.enc[rf_panel.enc_id[r]] += s * rf_panel.enc_dir[r];
        }
    rf_tick(++lb_now);
    if (frame)
        rf_frame();
    else
        rf_idle();
}
void lb_init(uint32_t ms)
{
    static float sink[2 * 64];
    uint32_t i, due = 0, done = 0;
    rf_init(0);
    for (i = 0; i < ms; i++) {           /* the start screen, silently, the audio drawn as it falls due */
        lb_ms(i % 15u == 0u);
        due = (uint32_t)((uint64_t)(i + 1u) * 44100u / 1000u);
        while (done < due) {
            uint32_t n = due - done > 64u ? 64u : due - done;
            lb_audio(sink, n);
            done += n;
        }
    }
}
/* the render's own 24-bit blocks (sim_render, APP_BLOCK frames), to float: no 16-bit step (rf_audio's) on the PC */
static int32_t lb_blk[2 * APP_BLOCK];
static uint32_t lb_blk_at = APP_BLOCK;
void lb_audio(float *lr, uint32_t frames)
{
    while (frames) {
        uint32_t n, k;
        if (lb_blk_at >= APP_BLOCK) {
            if (rf_booted && !rf_halted)
                sim_render(lb_blk);
            else
                memset(lb_blk, 0, sizeof lb_blk);
            lb_blk_at = 0;
        }
        n = APP_BLOCK - lb_blk_at < frames ? APP_BLOCK - lb_blk_at : frames;
        for (k = 0; k < 2u * n; k++)
            lr[k] = (float)lb_blk[2u * lb_blk_at + k] * (1.0f / 8388608.0f);
        lb_blk_at += n;
        lr += 2u * n;
        frames -= n;
    }
}
int lb_midi_in(uint32_t pkt) { return rf_midi_in(pkt); }
int lb_midi_out(uint32_t *pkt) { return rf_midi_out_take(pkt); }
void lb_button(int b, int down)
{
    uint32_t bit;
    if (b < 0 || b >= RF_NB)
        return;
    bit = 1u << rf_panel.btn_id[b];
    if (down) {
        rf_panel.buttons |= bit;
        rf_panel.buttons_tap |= bit;     /* (a press shorter than a millisecond still counts) */
    } else
        rf_panel.buttons &= ~bit;
}
void lb_key(int k, int down, int vel)
{
    uint32_t bit;
    if (k < 0 || k >= RF_NKEY)
        return;
    bit = 1u << k;
    rf_panel.key_vel[k] = (uint8_t)(vel > 127 ? 127 : vel < 1 ? 100 : vel);
    if (down) {
        rf_panel.keys |= bit;
        rf_panel.keys_tap |= bit;
    } else
        rf_panel.keys &= ~bit;
}
void lb_turn(int role, int detents)
{
    if (role < 0 || role >= RF_NE - 1)
        return;
    if (role < 3)
        lb_nav[role] += detents;
    else
        rf_panel.enc[rf_panel.enc_id[role]] += detents * rf_panel.enc_dir[role];
}
void lb_turn_hw(int role, int detents)
{
    if (role < 0 || role >= RF_NE - 1 || !detents)
        return;
    rf_panel.enc[rf_panel.enc_id[role]] += detents * rf_panel.enc_dir[role];
    if (role == 0)
        lb_hw_sel = 1u;
}
void lb_tempo(int detents)
{
    char t[16];
    if (!detents)
        return;
    if (ext_active()) {                  /* (the host's clock has the tempo) */
        app_say("TEMPO", "EXT CLOCK");
        return;
    }
    set.tempo_x10 = (uint16_t)clamp((int32_t)set.tempo_x10 + detents * 10, 400, 3000);
    changed();
    tenths_text(set.tempo_x10, t);
    app_say("BPM", t);
}
void lb_master(int v) { rf_panel.master = v < 0 ? 0 : v > 1023 ? 1023 : v; }
uint32_t lb_screen(uint16_t *px)
{
    uint32_t i;
    if (px)
        for (i = 0; i < RF_LCD_W * RF_LCD_H; i++) {
            uint16_t v = rf_panel.lcd[i];
            px[i] = (uint16_t)(v >> 8 | v << 8);   /* (the panel's high byte first: native order here) */
        }
    return rf_panel.lcd_writes;
}
uint32_t lb_leds(uint8_t *out, uint32_t n)
{
    uint32_t m = sizeof rf_panel.led < n ? (uint32_t)sizeof rf_panel.led : n;
    memcpy(out, rf_panel.led, m);
    return m;
}
void lb_lights(uint32_t *o)
{
    o[0] = lights_b;
    o[1] = lights_glow > 0 ? lights_db : 0u;
    o[2] = lit_keys;
    o[3] = mid_keys;
    o[4] = dim_keys;
    o[5] = (uint32_t)lights_glow;
}
int lb_playing(void) { return playing_now(); }
uint32_t lb_flash_size(void) { return RF_FLASH_SIZE; }
uint32_t lb_flash_get(uint8_t *out, uint32_t n)
{
    rf_flash_sync();
    if (n > RF_FLASH_SIZE)
        n = RF_FLASH_SIZE;
    memcpy(out, rf_flash(), n);
    return n;
}
int lb_flash_set(const uint8_t *in, uint32_t n) { return rf_flash_stage(in, n); }
int lb_breaks(void) { return (int)n_breaks; }
/* (the VST's break browser and playlist: FLEXOR, "une liste déroulante digeste avec des sous-menus ... une playlist de
 * breaks"): each break's name, and one asked for (at the next bar while playing, as ALGORITHM) */
int lb_break_name(int i, char *out, int cap)
{
    const char *n;
    int k = 0;
    if (i < 0 || i >= (int)n_breaks || cap < 1)
        return 0;
    n = break_name((uint32_t)i);
    while (n[k] && k < cap - 1) { out[k] = n[k]; k++; }
    out[k] = 0;
    return 1;
}
void lb_break_go(int i) { if (i >= 0 && i < (int)n_breaks) break_ask((uint32_t)i); }   /* breaks in all (the bank's, the player's) */
void lb_save(void) { store_all(); }   /* the FM-1's SAVE, without its screen: into the flash image */
void lb_sync(int on) { set.clk_in = (uint8_t)(on != 0); }   /* follow the MIDI clock in (the host's) */
LB_API uint32_t lb_debug(void) { return (uint32_t)set.tut | (uint32_t)set.novice << 8 | (uint32_t)ui.page << 16 | (uint32_t)(ui.lobo_t != 0) << 24; }
/* a few of the firmware's values, for the tests and the window: 0 the break playing, 1 the breaks, 2 the per-break data
 * saved (bytes), 3 the break's length (samples), 4 its rate */
LB_API int lb_get(int what)
{
    const brk_break_t *b = brk_get_break();
    switch (what) {
    case 0: return set.brk;
    case 1: return (int)n_breaks;
    case 2: return kx2_size();
    case 3: return b ? (int)b->len : 0;
    case 4: return b ? (int)b->rate : 0;
    case 5: return kx2.chaos[0];
    }
    return -1;
}

/* ------------------------------------------------- the VST's own interface: the page's model (lb_model) --- */
static lb_model_t *lb_cap;               /* (the model being filled while draw_tabs runs; 0 otherwise) */
static void lb_copy(char *d, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; s && s[i] && i + 1u < n; i++)
        d[i] = s[i];
    d[i] = 0;
}
static void lb_cap_tab(const char *s, int on)
{
    if (!lb_cap || lb_cap->ntabs >= LB_TABS)
        return;
    lb_copy(lb_cap->tabs[lb_cap->ntabs], s, sizeof lb_cap->tabs[0]);
    if (on)
        lb_cap->tab_on = lb_cap->ntabs;
    lb_cap->ntabs++;
}
static void lb_cap_part(const char *label, const char *part)
{
    if (!lb_cap)
        return;
    lb_copy(lb_cap->label, label, sizeof lb_cap->label);
    lb_copy(lb_cap->part, part, sizeof lb_cap->part);
}
void lb_model(lb_model_t *m)
{
    brk_status_t st;
    uint32_t i;
    char t[24];
    memset(m, 0, sizeof *m);
    m->tab_on = -1;
    lb_copy(m->dom, dom_name(), sizeof m->dom);
    lb_cap = m;
    draw_tabs();                         /* (the tab strip again: its tabs and part noted; the same pixels) */
    lb_cap = 0;
    if (!m->part[0] && (ui.page == PG_KICK || ui.page == PG_KEQ || IS_KFX()) && !kick_easy())
        kick_part_name(kick_part(), m->part);   /* (the screen leaves it out when the tabs take the room) */
    else if (IS_KFX())                   /* (a kick's FX1 / FX2: its effect, or the part of it ALGORITHM is on) */
        lb_copy(m->part, fx_part_name()[0] ? fx_part_name() : fx_name(fx_foc()), sizeof m->part);
    else if (!m->part[0] && ui.page == PG_PERC && !set.novice)
        lb_copy(m->part, PPAGE_NAME[perc_pg()], sizeof m->part);
    brk_status(&st);
    m->sel = ui.sel_foot != 0;
    for (i = 0; i < 4u; i++) {
        t[0] = 0;
        if (m->sel) {
            lb_copy(m->kname[i], SK_NAME[selk_fn(i)], sizeof m->kname[i]);
            selk_value(selk_fn(i), 0, t);
            m->knorm[i] = -1.0f;
        } else {
            lb_copy(m->kname[i], knob_name((int)i), sizeof m->kname[i]);
            knob_value((int)i, t);
            m->knorm[i] = foot_norm((int)i);
        }
        lb_copy(m->kval[i], t, sizeof m->kval[i]);
    }
    for (i = 0; i < 12u; i++) {
        uint32_t f = bk_fn_shown(i);
        if (i == 11u && !rec_key_on()) {
            lb_copy(m->pad[i], "REC", sizeof m->pad[i]);
            lb_copy(m->padlong[i], "RECORD", sizeof m->padlong[i]);
            m->padon[i] = ui.rec != 0;
            continue;
        }
        lb_copy(m->pad[i], BKF_NAME[f % BKF_N], sizeof m->pad[i]);
        lb_copy(m->padlong[i], BKF_LONG[f % BKF_N], sizeof m->padlong[i]);
        m->padon[i] = f == BKF_ARP && i < 11u ? arpk_on(i) : bkf_lit(f, &st);
    }
    m->playing = playing_now();
    m->step = st.step;
    m->bar16 = st.bar16;
    m->slice = st.slice;
    m->key = st.key;
    m->pos = st.pos;
    m->page = ui.page;
    m->novice = set.novice;
    m->bpm = (float)set.tempo_x10 * 0.1f;
    m->pattern = (int)pat_cur() + 1;
    lb_copy(m->brk, break_name(set.brk), sizeof m->brk);
    {                                    /* the kick and the drums, for the VST's pictures' cards */
        uint32_t p = kpre_mem(set.kslot);
        m->kslot = (int)(set.kslot % KICK_MEMS) + 1;
        if (p < KICK_PRESETS)
            lb_copy(m->kpre, KICK_PRESET_NAME[p], sizeof m->kpre);
        else
            lb_copy(m->kpre, "COPY", sizeof m->kpre);
        for (i = 0; i < KICK_LAYERS; i++)
            m->klev[i] = set.kick.l[i].level;
        m->klev[4] = (float)kx2_at(set.kslot)->tok * 0.01f;
        for (i = 0; i < 3u; i++) {
            lb_copy(m->dpre[i], pp_name(i, perc_x(i)[PX_PRE]), sizeof m->dpre[i]);
            m->dlev[i] = perc_voice(i)->level;
        }
    }
    if (plat_ms() < ui.msg_until) {
        lb_copy(m->say[0], ui.msg[0], sizeof m->say[0]);
        lb_copy(m->say[1], ui.msg[1], sizeof m->say[1]);
    }
    m->say_n = ui.msg_until;
    m->skin = (int)skin_idx();
}

/* the break playing's waveform: n columns, each its peak and its low / mid / high energy (one-pole splits, as the
 * strip's wave_build does), normalised; its slices' starts in 0..1 */
uint32_t lb_wave(float *cols, int n, float *slices, int *nslices)
{
    static uint32_t serial;
    static const int16_t *was_pcm;
    static uint32_t was_len;
    const brk_break_t *b = brk_get_break();
    uint32_t i, c;
    float lp1 = 0.0f, lp2 = 0.0f, mx[4] = {1e-9f, 1e-9f, 1e-9f, 1e-9f};
    if (!b || !b->len)
        return serial;
    if (b->pcm != was_pcm || b->len != was_len) {
        was_pcm = b->pcm, was_len = b->len;
        serial++;
    }
    if (!cols || n <= 0)                 /* (the serial alone: has the break changed) */
        return serial;
    for (c = 0; c < (uint32_t)n; c++) {
        uint32_t a = (uint32_t)((uint64_t)b->len * c / (uint32_t)n), z = (uint32_t)((uint64_t)b->len * (c + 1u) / (uint32_t)n);
        float pk = 0.0f, lo = 0.0f, mi = 0.0f, hi = 0.0f;
        for (i = a; i < z; i++) {
            float x = (float)b->pcm[i] * (1.0f / 32768.0f), ax = x < 0.0f ? -x : x;
            lp1 += (x - lp1) * 0.03f;    /* (~200 Hz) */
            lp2 += (x - lp2) * 0.25f;    /* (~2 kHz) */
            if (ax > pk) pk = ax;
            lo += lp1 * lp1;
            mi += (lp2 - lp1) * (lp2 - lp1);
            hi += (x - lp2) * (x - lp2);
        }
        if (z > a) {
            lo = sqrtf(lo / (float)(z - a)), mi = sqrtf(mi / (float)(z - a)), hi = sqrtf(hi / (float)(z - a));
        }
        cols[4 * c] = pk, cols[4 * c + 1] = lo, cols[4 * c + 2] = mi, cols[4 * c + 3] = hi;
        for (i = 0; i < 4u; i++)
            if (cols[4 * c + i] > mx[i]) mx[i] = cols[4 * c + i];
    }
    for (c = 0; c < (uint32_t)n; c++)
        for (i = 0; i < 4u; i++)
            cols[4 * c + i] /= mx[i];
    *nslices = (int)(b->nslices < 32u ? b->nslices : 32u);
    for (i = 0; i < (uint32_t)*nslices; i++)
        slices[i] = (float)b->slice[i] / (float)b->len;
    return serial;
}

/* ---- the pictures (FLEXOR, 25 Nov 2026: "dans le menu kick un vrai visuel sur le kick, une vraie waveform"): the
 * sound as the FM-1 draws it (its own kick_rgb / kick_wave, perc_rgb), as many columns as the window wants; called from
 * the plugin's picture thread (they read the sound's settings, write nothing of the core's) */
#define LB_PIC_MAX 2048
uint32_t lb_pic_sig(int what)
{
    if (what <= 0) {
        uint32_t s = sig_of(&set.kick, sizeof set.kick) ^ sig_of(set.ksyn, sizeof set.ksyn) * 3u;
        return s ^ (sig_of(&klab.cur, sizeof klab.cur) ^ sig_of(kx2_at(set.kslot), sizeof(kick_x2_t)) * 5u) * 7u;
    }
    return sig_of(perc_voice((uint32_t)(what - 1) % 3u), sizeof(perc_voice_t)) * 11u + (uint32_t)what;
}
int lb_pic(int what, int n, int ms, float *out)
{
    static uint8_t col[LB_PIC_MAX][4];
    static int8_t px[LB_PIC_MAX];
    int i;
    if (!out || n <= 0)
        return 0;
    n = n > LB_PIC_MAX ? LB_PIC_MAX : n;
    ms = ms < 20 ? 20 : ms > 3000 ? 3000 : ms;
    if (what <= 0) {
        kick_t kk = set.kick;            /* (the whole kick, as heard; nothing of the core's written: another thread) */
        kick_rgb(&kk, col, (uint32_t)n, (uint32_t)ms);
        kick_wave(&kk, px, (uint32_t)n, (uint32_t)ms, 127u);
    } else {
        perc_rgb((uint32_t)(what - 1) % 3u, perc_voice((uint32_t)(what - 1) % 3u), col, (uint32_t)n, (uint32_t)ms);
        memset(px, 0, sizeof px);
    }
    for (i = 0; i < n; i++) {
        out[5 * i] = (float)px[i] * (1.0f / 127.0f);
        out[5 * i + 1] = (float)col[i][0] * (1.0f / 255.0f);
        out[5 * i + 2] = (float)col[i][1] * (1.0f / 255.0f);
        out[5 * i + 3] = (float)col[i][2] * (1.0f / 255.0f);
        out[5 * i + 4] = (float)col[i][3] * (1.0f / 255.0f);
    }
    return n;
}
/* the break playing's min and max, column by column (the detailed waveform: 2 floats a column, -1..1) */
int lb_wave_mm(float *mm, int n)
{
    const brk_break_t *b = brk_get_break();
    int c;
    uint32_t i;
    if (!b || !b->len || !mm || n <= 0)
        return 0;
    for (c = 0; c < n; c++) {
        uint32_t a = (uint32_t)((uint64_t)b->len * (uint32_t)c / (uint32_t)n), z = (uint32_t)((uint64_t)b->len * (uint32_t)(c + 1) / (uint32_t)n);
        int16_t lo = 0, hi = 0;
        for (i = a; i < z; i++) {
            if (b->pcm[i] < lo) lo = b->pcm[i];
            if (b->pcm[i] > hi) hi = b->pcm[i];
        }
        mm[2 * c] = (float)lo * (1.0f / 32768.0f), mm[2 * c + 1] = (float)hi * (1.0f / 32768.0f);
    }
    return n;
}
/* a sound heard alone: 0 the kick, 1..3 SN HH CP */
void lb_drum_hit(int d) { if (d <= 0) kick_hit(); else perc_hit((uint32_t)(d - 1) % 3u); }

int lb_key_index(int black, int i) { return black ? (i >= 0 && i < 11 ? BK[i] : -1) : (i >= 0 && i < 16 ? WK[i] : -1); }

/* ---- the sequencer (lbcore.h lb_seq, lb_step, lb_patterns) ---- */
void lb_seq(lb_seq_t *q)
{
    const pat_t *p = pat_now();
    brk_status_t st;
    uint32_t d, b, i;
    uint8_t lane = ui.seq_lane;
    brk_status(&st);
    memset(q, 0, sizeof *q);
    q->pat = (int)pat_cur();
    q->next = pat_next >= 0 ? pat_next : -1;
    q->playing = playing_now();
    q->bar64 = st.playing && st.bar64 >= 0 ? st.bar64 : -1;
    q->bstep = st.playing && st.step >= 0 ? (int)(st.step % (int32_t)PAT_STEPS) : -1;
    q->slices = (int)n_slices();
    q->blen = (int)pat_len();
    for (d = 0; d < NDRUMS; d++) {
        q->bars[d] = (int)drum_bars(p, d);
        for (b = 0; b < 4u; b++)
            q->mask[d][b] = (uint16_t)drum_bar(p, d, b);
        for (i = 0; i < (d == DR_KICK ? 64u : 32u); i++)
            q->sb[d][i] = *stepb(set.pattern, d, i);
        ui.seq_lane = (uint8_t)(SL_KICK + d * 4u);
        lane_sound_text(q->snd[d]);
    }
    ui.seq_lane = lane;
    q->novice = set.novice;
    q->changed = grp_changed();
    q->clip = clip_from;
    for (i = 0; i < 64u; i++)
        q->note[i] = (int8_t)kick_note(p, i);
    for (i = 0; i < PAT_STEPS && i < 32u; i++) {
        q->brk[i][0] = p->st[i][0];
        q->brk[i][1] = p->st[i][1];
        q->brk[i][2] = p->st[i][2];
    }
    {
        const char *n = break_name(set.brk);
        for (i = 0; i < 15u && n[i]; i++)
            q->brkname[i] = n[i];
    }
}
int lb_step(int op, int a, int b, int c, int d)
{
    uint8_t lane = ui.seq_lane, nov = set.novice;
    int ok = 1;
    if (op >= 1 && op <= 6) {
        uint8_t u[5] = { (uint8_t)op, (uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)(int8_t)d };
        return ed_step(u);
    }
    set.novice = 0u;
    if (op == 7) {                       /* copy: this pattern into the clipboard (pat_copy_paste's first half) */
        clip_from = -1;
        pat_copy_paste();
    } else if (op == 8) {                /* paste: the clipboard here (another pattern) */
        if (clip_from < 0 || (uint32_t)clip_from == pat_cur())
            ok = 0;
        else
            pat_copy_paste();
    } else if (op == 9 && d) {
        ui.seq_lane = (uint8_t)(a >= 4 ? 0u : SL_KICK + (uint32_t)a * 4u);
        seq_rotate(d);
    } else if (op == 10) {
        ui.seq_lane = (uint8_t)(a >= 4 ? (uint32_t)b % 2u : SL_KICK + (uint32_t)a * 4u + (uint32_t)b % 4u);
        seq_copy_bar((uint32_t)c);
    } else if (op == 11)
        seq_clear_drums();
    else if (op == 12) {
        ui.seq_lane = 0u;
        seq_clear(2);                    /* (REC held on SEQ's: both halves of the break, the drums kept) */
    } else if (op == 13)
        seq_clear(0);                    /* (the whole pattern, SEL + REC's) */
    else
        ok = 0;
    ui.seq_lane = lane;
    set.novice = nov;
    seq_lane_fix();
    ui.redraw |= RG_ALL;
    return ok;
}
void lb_patterns(uint8_t *out)
{
    static uint8_t o[2u + 16u * 40u];
    uint32_t g;
    for (g = 0; g < NGRP; g++) {
        edx_bank(g, o);
        memcpy(out + g * 16u * 40u, o + 2, 16u * 40u);
    }
}

/* ---- the pages as menus (lbcore.h lb_scan, lb_options): looked at ahead through the firmware's own code, the core
 * then put back exactly as it was (lbsnap.c) ---- */
static uint8_t *lb_snapbuf;
static void lb_tick1(void)               /* one millisecond, its input read, no frame */
{
    rf_tick(++lb_now);
    rf_idle();
}
static void lb_enc(int role, int d)      /* a detent of an encoder, as the panel turns it, read at once */
{
    rf_panel.enc[rf_panel.enc_id[role]] += d * rf_panel.enc_dir[role];
    lb_tick1();
}
static void lb_part_of(lb_part_t *p)
{
    lb_model_t m;
    int i;
    lb_model(&m);
    memset(p, 0, sizeof *p);
    lb_copy(p->part, m.part, sizeof p->part);
    for (i = 0; i < 4; i++) {
        lb_copy(p->kname[i], m.kname[i], sizeof p->kname[i]);
        lb_copy(p->kval[i], m.kval[i], sizeof p->kval[i]);
        p->knorm[i] = m.knorm[i];
    }
}
static int lb_same_part(const lb_part_t *a, const lb_part_t *b)
{
    int i;
    if (strcmp(a->part, b->part))
        return 0;
    for (i = 0; i < 4; i++)
        if (strcmp(a->kname[i], b->kname[i]))
            return 0;
    return 1;
}
static int lb_snap_ready(void)
{
    if (!lb_snapbuf)
        lb_snapbuf = snap_new();
    return lb_snapbuf != 0;
}
int lb_scan(lb_part_t *out, int max)
{
    lb_model_t m;
    int n = 1;
    if (max < 1)
        return 0;
    lb_part_of(&out[0]);
    lb_model(&m);
    if (!m.part[0] || m.sel || !lb_snap_ready())   /* (no parts: ALGORITHM is the break there; SEL held: SEL's knobs) */
        return 1;
    snap_take(lb_snapbuf);
    while (n < max) {
        lb_enc(LB_ALGO, 1);
        lb_part_of(&out[n]);
        if (lb_same_part(&out[n], &out[0]))
            break;
        n++;
    }
    snap_put(lb_snapbuf);
    return n;
}
/* the effects PRESETS goes through on an effect's page (a kick's FX1 / FX2, a slot of the chain): their names in order,
 * the one in use; looked at ahead, the core put back. 0: not an effect's page */
int lb_fx_list(char (*names)[16], int max, int *cur)
{
    char was[16];
    int n = 0, still = 0, i;
    if (ui.page != PG_FX || !lb_snap_ready() || max < 1)
        return 0;
    lb_copy(was, fx_name(fx_foc()), sizeof was);
    snap_take(lb_snapbuf);
    for (i = 0; i < 40; i++)             /* (to the first: PRESETS stops at the ends) */
        lb_enc(LB_PRESET, -1);
    lb_copy(names[n++], fx_name(fx_foc()), 16);
    while (n < max && still < 3) {
        lb_enc(LB_PRESET, 1);
        if (!strcmp(fx_name(fx_foc()), names[n - 1])) { still++; continue; }
        if (!strcmp(fx_name(fx_foc()), names[0])) break;   /* (round) */
        still = 0;
        lb_copy(names[n++], fx_name(fx_foc()), 16);
    }
    snap_put(lb_snapbuf);
    *cur = 0;
    for (i = 0; i < n; i++)
        if (!strcmp(names[i], was)) *cur = i;
    return n;
}
static int lb_opt_walk(int knob, int d, char (*v)[LB_OPT_LEN], int max, int *wrap, const char *start)
{
    char now[24], last[24];
    int n = 0, still = 0;
    lb_copy(last, start, sizeof last);
    while (n < max) {
        lb_model_t m;
        lb_enc(LB_K1 + knob, d);
        lb_model(&m);
        lb_copy(now, m.kval[knob], sizeof now);
        if (!strcmp(now, last)) {        /* (unchanged: the end, or a knob that wants more than a detent) */
            if (++still >= 3)
                break;
            continue;
        }
        still = 0;
        if (!strcmp(now, start)) {       /* (back where it began: a list that goes round) */
            *wrap = 1;
            break;
        }
        lb_copy(v[n++], now, LB_OPT_LEN);
        lb_copy(last, now, sizeof last);
    }
    return n;
}
int lb_options(int part, int knob, lb_opts_t *o)
{
    static char up[LB_OPT_MAX][LB_OPT_LEN], dn[LB_OPT_MAX][LB_OPT_LEN];   /* (overwritten by the putting back: copied
                                          * out first) */
    char start[24];
    int i, nu, nd = 0, wrap = 0, half = LB_OPT_MAX / 2 - 1;
    lb_model_t m;
    memset(o, 0, sizeof *o);
    if (knob < 0 || knob > 3 || !lb_snap_ready())
        return 0;
    snap_take(lb_snapbuf);
    for (i = 0; i < part; i++)
        lb_enc(LB_ALGO, 1);
    lb_model(&m);
    lb_copy(start, m.kval[knob], sizeof start);
    nu = lb_opt_walk(knob, 1, up, half, &wrap, start);
    {
        char keep[LB_OPT_MAX / 2][LB_OPT_LEN];   /* (on the stack: outside what is put back) */
        int ku = nu;
        memcpy(keep, up, (size_t)nu * LB_OPT_LEN);
        snap_put(lb_snapbuf);
        if (!wrap) {
            for (i = 0; i < part; i++)
                lb_enc(LB_ALGO, 1);
            nd = lb_opt_walk(knob, -1, dn, half, &wrap, start);
            {
                char keepd[LB_OPT_MAX / 2][LB_OPT_LEN];
                memcpy(keepd, dn, (size_t)nd * LB_OPT_LEN);
                snap_put(lb_snapbuf);
                for (i = 0; i < nd; i++)
                    memcpy(o->v[i], keepd[nd - 1 - i], LB_OPT_LEN);
            }
        }
        o->cur = nd;
        memcpy(o->v[nd], start, LB_OPT_LEN - 1);
        for (i = 0; i < ku; i++)
            memcpy(o->v[nd + 1 + i], keep[i], LB_OPT_LEN);
        o->n = nd + 1 + ku;
    }
    o->wrap = wrap;
    o->more = nu >= half || nd >= half;
    return o->n;
}
/* a value set as the menus ask: part p's knob k moved `steps` values (each a change of its value, whatever detents
 * that takes), then back to the part that was shown; the firmware's own code all along (its clamps, its saves) */
int lb_set(int part, int knob, int steps)
{
    lb_model_t m;
    char last[24];
    int i, done = 0, d = steps > 0 ? 1 : -1;
    if (knob < 0 || knob > 3)
        return 0;
    for (i = 0; i < part; i++)
        lb_enc(LB_ALGO, 1);
    lb_model(&m);
    lb_copy(last, m.kval[knob], sizeof last);
    while (done < (steps < 0 ? -steps : steps)) {
        int tries = 0, moved = 0;
        while (tries++ < 4) {
            lb_enc(LB_K1 + knob, d);
            lb_model(&m);
            if (strcmp(m.kval[knob], last)) {
                lb_copy(last, m.kval[knob], sizeof last);
                moved = 1;
                break;
            }
        }
        if (!moved)
            break;                       /* (the end: nothing further) */
        done++;
    }
    for (i = 0; i < part; i++)
        lb_enc(LB_ALGO, -1);
    return done;
}
/* a kick heard alone (FLEXOR: "écouter sur le PC les kicks de la machine que je vais remplacer"): the blob put in as
 * the kick playing, the transport stopped, one hit rendered through the firmware's own engine and output (its EQ, FX,
 * the limiter), `frames` stereo frames at 44.1 kHz; then the core exactly as it was. The frames rendered, 0 failed */
uint32_t lb_kick_render(const uint8_t *blob, float *lr, uint32_t frames)
{
    sy_kobjs_t o;
    sy_kick_t c;
    uint32_t i;
    if (!sy_kblob_ok(blob) || !lb_snap_ready())
        return 0;
    memcpy(&c, blob + 8, sizeof c);
    snap_take(lb_snapbuf);
    brk_play(0);
    o.S = &set, o.ST = &steps, o.K2 = &km2, o.KL = &klab, o.KF = &kfxm, o.X = kx2.m;
    sy_kick_put(&o, set.kslot % KICK_MEMS, &c);
    keq_flat = 0u;
    changed_kick();
    changed_fx();
    for (i = 0; i < 40u; i++)            /* (its settings to the engine, the stop's tails out) */
        lb_tick1();
    {
        static float sink[2 * 256];
        for (i = 0; i < 8u; i++)
            lb_audio(sink, 256);         /* (what still rang: out) */
    }
    kick_hit();
    lb_tick1();
    lb_audio(lr, frames);
    snap_put(lb_snapbuf);
    return frames;
}

/* ---- the song (the VST's arranger: FLEXOR, "penser la VST comme un mini DAW"): a track read whole, written whole
 * (each item checked, as the Studio's), played, stopped, picked: the firmware's edx_track, edx_track_write, track_play */
void lb_track(int t, lb_track_t *o)
{
    static uint8_t b[256];
    uint32_t n = edx_track((uint32_t)t % NTRACKS, b), k = 7;
    memset(o, 0, sizeof *o);
    if (n < 7u + 4u * TRK_ITEMS + 2u)
        return;
    o->track = b[1], o->cur = b[2], o->on = b[3], o->pos = b[4], o->end = b[5], o->from = b[6];
    memcpy(o->it, b + k, TRK_ITEMS); k += TRK_ITEMS;
    memcpy(o->rep, b + k, TRK_ITEMS); k += TRK_ITEMS;
    memcpy(o->tb, b + k, TRK_ITEMS); k += TRK_ITEMS;
    memcpy(o->tk, b + k, TRK_ITEMS); k += TRK_ITEMS;
    o->patbars = b[k], o->dropbars = b[k + 1];
}
void lb_track_write(const lb_track_t *t)
{
    static uint8_t u[4 + 4 * TRK_ITEMS];
    u[0] = 4u, u[1] = (uint8_t)(t->track % NTRACKS), u[2] = (uint8_t)t->end, u[3] = (uint8_t)t->from;
    memcpy(u + 4, t->it, TRK_ITEMS);
    memcpy(u + 4 + TRK_ITEMS, t->rep, TRK_ITEMS);
    memcpy(u + 4 + 2 * TRK_ITEMS, t->tb, TRK_ITEMS);
    memcpy(u + 4 + 3 * TRK_ITEMS, t->tk, TRK_ITEMS);
    edx_track_write(u, sizeof u);
}
void lb_track_play(int op, int t)        /* 1 play track t, 2 stop, 3 pick it */
{
    if (op == 1) {
        set.trk = (uint8_t)((uint32_t)t % NTRACKS);
        track_play(0);
    } else if (op == 2 && ui.trk_on)
        track_stop("STOPPED");
    else if (op == 3) {
        set.trk = (uint8_t)((uint32_t)t % NTRACKS);
        ui.trk_cur = 0u;
        changed();
    }
}
