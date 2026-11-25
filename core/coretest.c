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
    printf("%s\n", bad ? "FAILED" : "OK");
    return bad != 0;
}
