/* LOAF BREAKER VST: the core's menus and sequencer checked, as the plugin calls them (CI, Linux and Windows: the state
 * kept and put back by lbsnap.c is the platform's own code). Exit 0: all good. */
#include <stdio.h>
#include <string.h>
#include "lbcore.h"
static int bad;
static void run(int ms) { for (int i = 0; i < ms; i++) lb_ms(i % 15 == 0); }
static void tap(int b) { lb_button(b, 1); run(2); lb_button(b, 0); run(80); }
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); bad += !ok; }
static lb_opts_t o;
int main(void)
{
    lb_part_t p[32], q[32];
    lb_model_t a, b;
    lb_seq_t s;
    int n, i;
    lb_init(5000);
    run(200);
    tap(LB_PLAY);
    run(300);
    {
        int was = lb_playing();
        printf("playing at the start: %d\n", was);
    }
    tap(LB_EDIT);
    run(200);
    lb_model(&a);
    n = lb_scan(p, 32);
    check(n >= 4, "the KICK's ATTACK: its parts scanned");
    lb_model(&b);
    check(!strcmp(a.part, b.part) && !memcmp(a.kval, b.kval, sizeof a.kval), "the page as it was after the scan");
    for (i = 0; i < n; i++)
        if (!strcmp(p[i].part, "PUNCH"))
            break;
    check(i < n, "PUNCH found");
    check(lb_options(i, 0, &o) > 10 && !o.wrap, "PUNCH LEVEL: its values");
    lb_model(&b);
    check(!memcmp(a.kval, b.kval, sizeof a.kval), "the page as it was after the values");
    check(lb_set(i, 0, 999) > 0, "PUNCH LEVEL to its end");
    lb_scan(q, 32);
    check(!strcmp(q[i].kval[0], "100%"), "PUNCH LEVEL 100%");
    check(lb_set(i, 0, 1) == 0, "nothing past the end");
    tap(LB_SEQ);
    run(200);
    check(lb_step(1, 0, 0, 0, 0) == 1, "a kick step on");
    run(30);
    lb_seq(&s);
    check(s.mask[0][0] & 1, "the kick's step 1 on");
    run(500);
    lb_model(&b);
    check(!strcmp(b.dom, "PATTERN"), "still on PATTERN, still answering");
    {                                    /* the pick and the library: an FM-1 backup (the VST's own, exported), a kick into
                                          * it, heard, the rest as it was */
        static uint8_t img[200000], out[200000], blob[8192];
        static float lr[2 * 22050];
        char rep[512];
        lb_list_t l0, l1;
        lb_pick_t pk = { LB_PICK_KICK, 6, 2, 0 };
        uint32_t m = lb_fm1_export(img, sizeof img, rep, sizeof rep), m2;
        check(m > 0 && lb_fm1_list(img, m, &l0) == 0, "a backup listed");
        m2 = lb_fm1_put(img, m, &pk, 1, out, sizeof out, rep, sizeof rep);
        check(m2 > 0 && lb_fm1_list(out, m2, &l1) == 0 && !strcmp(l1.kick[2], l0.kick[6]) && !strcmp(l1.kick[3], l0.kick[3]),
              "K7 into its K3, K4 as it was");
        check(lb_kick_size() <= sizeof blob && lb_kick_read(out, m2, 2, blob), "a kick as a blob");
        lb_model(&a);
        check(lb_kick_render(blob, lr, 22050) == 22050, "a kick heard");
        {
            double e = 0;
            for (i = 0; i < 2 * 22050; i++) e += (double)lr[i] * lr[i];
            check(e > 1.0, "it sounds");
        }
        lb_model(&b);
        check(!strcmp(a.dom, b.dom) && !memcmp(a.kval, b.kval, sizeof a.kval), "the VST as it was after hearing it");
    }
    {                                    /* the song: a track read, an item added, read back */
        lb_track_t t0, t1;
        int n = 0;
        lb_track(1, &t0);
        t0.it[0] = 5, t0.rep[0] = 3, t0.it[1] = 0;
        lb_track_write(&t0);
        lb_track(1, &t1);
        while (n < 32 && t1.it[n]) n++;
        check(n == 1 && t1.it[0] == 5 && t1.rep[0] == 3, "a track written and read back");
    }
    {                                    /* the pictures: the kick drawn, its wave crossing zero; a drum; the break */
        static float o[1000 * 5], mm[400 * 2];
        int i, cross = 0;
        float pk = 0.0f;
        const uint32_t s0 = lb_pic_sig(0);
        check(lb_pic(0, 1000, 400, o) == 1000, "the kick's picture: 1000 columns");
        for (i = 1; i < 1000; i++) {
            cross += (o[5 * i] > 0.0f) != (o[5 * (i - 1)] > 0.0f);
            pk = o[5 * i + 4] > pk ? o[5 * i + 4] : pk;
        }
        check(cross > 10 && pk > 0.3f, "the kick's picture: its cycles, its level");
        check(lb_pic_sig(0) == s0, "the kick's signature still while nothing changes");
        pk = 0.0f;
        lb_pic(1, 500, 400, o);
        for (i = 0; i < 500; i++) pk = o[5 * i + 4] > pk ? o[5 * i + 4] : pk;
        check(pk > 0.1f, "the snare's picture");
        check(lb_wave_mm(mm, 400) == 400 && mm[1] >= mm[0], "the break's min / max");
    }
    {                                    /* the lights, for the FM-1 as the controller: as the firmware gave them (the
                                          * page's buttons lit, the keys glowing at rest) */
        uint32_t l[6];
        lb_lights(l);
        check(l[0] != 0u && (l[2] | l[3] | l[4]) != 0u && l[5] <= 2u, "the lights");
    }
    {                                    /* a key's own settings (lb_key_knobs): a white key's PITCH turned, KIL's LATCH */
        lb_keyknobs_t kk;
        int ok = lb_key_knobs(0, 0, &kk) && kk.n == 4 && !strcmp(kk.name[1], "PITCH"), kil = -1;
        char was[20];
        strcpy(was, kk.val[1]);
        lb_key_knob_turn(0, 0, 1, 2);
        lb_key_knobs(0, 0, &kk);
        printf("     white key 1: PITCH %s -> %s\n", was, kk.val[1]);
        ok = ok && strcmp(was, kk.val[1]) != 0;
        lb_key_knob_turn(0, 0, 1, -2);
        for (i = 0; i < 12; i++)
            if (lb_key_knobs(1, i, &kk) && !strcmp(kk.fn, "KICKS OFF")) kil = i;
        if (kil >= 0) {
            lb_key_knobs(1, kil, &kk);
            strcpy(was, kk.val[0]);
            lb_key_knob_turn(1, kil, 0, strcmp(was, "ON") ? 1 : -1);
            lb_key_knobs(1, kil, &kk);
            printf("     KIL (black %d): %s %s -> %s\n", kil + 1, kk.name[0], was, kk.val[0]);
            ok = ok && !strcmp(kk.name[0], "LATCH") && strcmp(was, kk.val[0]) != 0;
            lb_key_knob_turn(1, kil, 0, strcmp(was, "ON") ? -1 : 1);
        } else
            printf("     (no KIL key in this layout)\n");
        check(ok, "a key's settings read and turned (white key, KIL's LATCH)");
    }
    {                                    /* the FM-1's SELECT (lb_turn_hw) the tempo on HOME, the VST's (lb_turn) never */
        lb_model_t m;
        float b0, b1, b2;
        for (i = 0; i < 3; i++) {
            lb_model(&m);
            if (!strcmp(m.dom, "LIVE")) break;
            lb_button(LB_HOME, 1); lb_ms(1); lb_button(LB_HOME, 0);
            for (int j = 0; j < 40; j++) lb_ms(j % 15 == 0);
        }
        lb_model(&m); b0 = m.bpm;
        lb_turn_hw(LB_SELECT, 2);
        for (i = 0; i < 40; i++) lb_ms(i % 15 == 0);
        lb_model(&m); b1 = m.bpm;
        lb_turn(LB_SELECT, 1);
        for (i = 0; i < 40; i++) lb_ms(i % 15 == 0);
        lb_model(&m); b2 = m.bpm;
        lb_turn(LB_SELECT, -1);
        for (i = 0; i < 40; i++) lb_ms(i % 15 == 0);
        lb_tempo(-2);
        printf("     tempo %.1f, FM-1 SELECT +2: %.1f, VST SELECT +1: %.1f (%s)\n", (double) b0, (double) b1, (double) b2, m.dom);
        check(b1 > b0 + 1.5f && b1 < b0 + 2.5f && b2 == b1, "the FM-1's SELECT the tempo, the VST's not");
    }
    printf("%s\n", bad ? "FAILED" : "OK");
    return bad != 0;
}
