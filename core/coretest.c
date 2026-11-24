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
    printf("%s\n", bad ? "FAILED" : "OK");
    return bad != 0;
}
