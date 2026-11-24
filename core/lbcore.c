/* LOAF BREAKER VST: the firmware's core (fm1-base's host/rfm1.c: the host simulator as a library, compiled natively
 * here, with the whole app: plat_host.c, app.c and its pages) and the C API of lbcore.h over its panel. */
#include <stdint.h>
const uint8_t *lb_bank_ptr;              /* (LB_BIG: the break bank the plugin loads, breaks.bank; app.c reads it) */
uint32_t lb_bank_len;
#include "rfm1.c"
#include "lbcore.h"
#include "fm1sync.c"
void lb_bank(const uint8_t *p, uint32_t n) { lb_bank_ptr = n >= 16u ? p : 0; lb_bank_len = n; }
int lb_abi(void) { return LB_ABI; }
static uint32_t lb_now;
void lb_ms(int frame)
{
    /* the first start's question (tutor.c TUT_ASK): EXPERT, white key 3, as a player would (the VST starts EXPERT) */
    if (set.tut == 0xFDu && !ui.lobo_t && !ui.intro && lb_now & 1u)   /* (a tap every other ms: a key going down) */
        rf_panel.keys_tap |= 1u << WK[2];
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
    rf_panel.enc[rf_panel.enc_id[role]] += detents * rf_panel.enc_dir[role];
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
int lb_breaks(void) { return (int)n_breaks; }   /* breaks in all (the bank's, the player's) */
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
    if (plat_ms() < ui.msg_until) {
        lb_copy(m->say[0], ui.msg[0], sizeof m->say[0]);
        lb_copy(m->say[1], ui.msg[1], sizeof m->say[1]);
    }
    m->say_n = ui.msg_until;
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

int lb_key_index(int black, int i) { return black ? (i >= 0 && i < 11 ? BK[i] : -1) : (i >= 0 && i < 16 ? WK[i] : -1); }
