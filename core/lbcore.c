/* LOAF BREAKER VST: the firmware's core (fm1-base's host/rfm1.c: the host simulator as a library, compiled natively
 * here, with the whole app: plat_host.c, app.c and its pages) and the C API of lbcore.h over its panel. */
#include "rfm1.c"
#include "lbcore.h"
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
    static int16_t sink[2 * 64];
    uint32_t i, due = 0, done = 0;
    rf_init(0);
    for (i = 0; i < ms; i++) {           /* the start screen, silently, the audio drawn as it falls due */
        lb_ms(i % 15u == 0u);
        due = (uint32_t)((uint64_t)(i + 1u) * 44100u / 1000u);
        while (done < due) {
            uint32_t n = due - done > 64u ? 64u : due - done;
            rf_audio(sink, n);
            done += n;
        }
    }
}
void lb_audio(float *lr, uint32_t frames)
{
    static int16_t b[2 * 256];
    while (frames) {
        uint32_t n = frames > 256u ? 256u : frames, k;
        rf_audio(b, n);
        for (k = 0; k < 2u * n; k++)
            lr[k] = (float)b[k] * (1.0f / 32768.0f);
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
void lb_save(void) { store_all(); }   /* the FM-1's SAVE, without its screen: into the flash image */
void lb_sync(int on) { set.clk_in = (uint8_t)(on != 0); }   /* follow the MIDI clock in (the host's) */
LB_API uint32_t lb_debug(void) { return (uint32_t)set.tut | (uint32_t)set.novice << 8 | (uint32_t)ui.page << 16 | (uint32_t)(ui.lobo_t != 0) << 24; }
