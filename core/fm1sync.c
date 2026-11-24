/* LOAF BREAKER VST: the VST <-> FM-1 translation (FLEXOR, 24 Nov 2026: "envoyer ce que j'ai fait sur le VST sur la
 * machine, un système de traduction et de synchronisation"). Included by lbcore.c after the app (it sees its types).
 * The FM-1's backup image (fm1-base backup.c's format, app.h APP_BACKUP_*: magic LBRK, the objects in order) made
 * from the VST's saved objects, and the other way. What differs between the two:
 *   - the breaks: the VST's bank is every break of the library (LB_BIG), the FM-1's its eight; a break is the same
 *     break by its name; one the FM-1 does not have goes to the FM-1's of the nearest tempo (reported), the FM-1's
 *     own breaks (after its bank) to the VST's first. Every field holding a break number is translated: set.brk,
 *     set.drop_brk, each DROP's (steps.dm[d][2]), each track item's (steps.tb, pattern items), each split's B
 *   - kx2 (the per-break white keys and splits, OBJ_KX2): 24 breaks on the FM-1, 128 on the VST, saved over four objects
 * Everything else is the same bytes on both. */
#define FM1_WK_NB 24u
typedef struct {                         /* app.c's kx2_t as the FM-1 has it (no LB_BIG) */
    uint32_t magic;
    kick_x2_t m[KICK_MEMS];
    uint8_t selk[11];
    uint8_t rsv[53];
    uint8_t sp_tag, split, split_b, split_bars;
    uint8_t sp[FM1_WK_NB];
    uint8_t ch_tag, chaos[4];
    uint8_t ch_sw;
    uint8_t rsv4[14];
    uint8_t selrec;
    uint8_t rsv3;
    uint16_t wk[FM1_WK_NB + 1u][BRK_KEYS];
    uint16_t wks[FM1_WK_NB][BRK_KEYS];
} kx2_fm1_t;
_Static_assert(offsetof(kx2_fm1_t, sp) == offsetof(kx2_t, sp), "kx2's head as the FM-1's");
_Static_assert(sizeof(kx2_fm1_t) <= PLAT_STORE_MAX, "the FM-1's kx2 is one object");

static uint32_t sy_crc(const uint8_t *p, uint32_t n, uint32_t c)
{
    uint32_t k;
    c = ~c;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static void sy_w16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void sy_w32(uint8_t *p, uint32_t v) { sy_w16(p, v); sy_w16(p + 2, v >> 16); }
static uint32_t sy_r16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t sy_r32(const uint8_t *p) { return sy_r16(p) | sy_r16(p + 2) << 16; }

/* the two banks: the FM-1's (built in, CH_BANK) and the VST's (the file), and each break's place in the other */
static uint32_t sy_nf, sy_nv;
static uint8_t sy_v2f[BANK_NSLOTS], sy_f2v[BANK_NSLOTS];
static uint8_t sy_used[BANK_NSLOTS];     /* (the VST's breaks a translation used: their approximations reported) */
static char *sy_rep;
static uint32_t sy_repn;
static void sy_say(const char *t)
{
    uint32_t n = (uint32_t)strlen(sy_rep);
    while (*t && n + 1u < sy_repn)
        sy_rep[n++] = *t++;
    sy_rep[n] = 0;
}
static const bank_slot_t *sy_fslot(uint32_t i) { return bank_check(CH_BANK, CH_BANK_SIZE, i); }
static const bank_slot_t *sy_vslot(uint32_t i) { return bank_check(BANK_PTR, BANK_LEN, i); }
static int sy_same(const bank_slot_t *a, const bank_slot_t *b) { return !strncmp(a->name, b->name, sizeof a->name); }
static void sy_maps(void)
{
    uint32_t i, j, best;
    for (sy_nf = 0; sy_nf < BANK_SLOTS && sy_fslot(sy_nf); sy_nf++)
        ;
    for (sy_nv = 0; sy_nv < BANK_NSLOTS && sy_vslot(sy_nv); sy_nv++)
        ;
    for (i = 0; i < sy_nv; i++) {        /* the VST's break i on the FM-1: the same name, else the nearest tempo */
        const bank_slot_t *v = sy_vslot(i);
        int32_t d, bd = 0x7FFFFFFF;
        best = 0;
        for (j = 0; j < sy_nf; j++) {
            const bank_slot_t *f = sy_fslot(j);
            d = (int32_t)f->bpm_x100 - (int32_t)v->bpm_x100;
            d = (sy_same(f, v) ? -1 : d < 0 ? -d : d);
            if (d < bd) {
                bd = d;
                best = j;
            }
        }
        sy_v2f[i] = (uint8_t)best;
    }
    for (j = 0; j < sy_nf; j++) {        /* the FM-1's break j in the VST: the same name, else the nearest tempo */
        const bank_slot_t *f = sy_fslot(j);
        int32_t d, bd = 0x7FFFFFFF;
        best = 0;
        for (i = 0; i < sy_nv; i++) {
            const bank_slot_t *v = sy_vslot(i);
            d = (int32_t)v->bpm_x100 - (int32_t)f->bpm_x100;
            d = (sy_same(f, v) ? -1 : d < 0 ? -d : d);
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
        sy_f2v[j] = (uint8_t)best;
    }
    memset(sy_used, 0, sizeof sy_used);
}
static uint32_t sy_to_fm1(uint32_t v)    /* a break's number, the VST's to the FM-1's */
{
    if (v >= sy_nv)
        return 0;
    sy_used[v] = 1;
    return sy_v2f[v];
}
static uint32_t sy_to_vst(uint32_t f) { return f < sy_nf ? sy_f2v[f] : 0u; }   /* (the FM-1's own breaks: the first) */
static void sy_report_export(void)
{
    uint32_t i, n = 0;
    char t[64];
    for (i = 0; i < sy_nv; i++)
        if (sy_used[i] && !sy_same(sy_vslot(i), sy_fslot(sy_v2f[i]))) {
            const bank_slot_t *v = sy_vslot(i), *f = sy_fslot(sy_v2f[i]);
            snprintf(t, sizeof t, "%s%.12s -> %.12s", n++ ? ", " : "BREAKS NOT ON THE FM-1: ", v->name, f->name);
            sy_say(t);
        }
    if (!n)
        sy_say("EVERY BREAK USED IS ON THE FM-1.");
}

/* the break fields of the three objects that hold some, in place: dir 0 the VST's to the FM-1's, 1 back */
static uint32_t sy_map(uint32_t b, int dir) { return dir ? sy_to_vst(b) : sy_to_fm1(b); }
static void sy_settings(uint8_t *p, uint32_t n, int dir)
{
    if (n > offsetof(settings_t, brk))
        p[offsetof(settings_t, brk)] = (uint8_t)sy_map(p[offsetof(settings_t, brk)], dir);
    if (n > offsetof(settings_t, drop_brk)) {
        uint32_t d = p[offsetof(settings_t, drop_brk)];
        if (d && d != DBRK_NEXT)
            p[offsetof(settings_t, drop_brk)] = (uint8_t)(sy_map(d - 1u, dir) + 1u);
    }
}
static void sy_steps(uint8_t *p, uint32_t n, const uint8_t *pt, uint32_t pn, int dir)
{
    steps_t *s = (steps_t *)p;
    const pats_t *pa = (const pats_t *)pt;
    uint32_t t, i, d;
    if (n >= offsetof(steps_t, kx) && pn >= offsetof(pats_t, trk) + sizeof pa->trk)
        for (t = 0; t < NTRACKS; t++)
            for (i = 0; i < TRK_ITEMS; i++) {
                uint32_t it = pa->trk[t].it[i];
                if (it != TI_END && it != TI_DROP && it != TI_GAP && s->tb[t][i])   /* (a pattern item's break) */
                    s->tb[t][i] = (uint8_t)(sy_map(s->tb[t][i] - 1u, dir) + 1u);
            }
    if (n >= offsetof(steps_t, dsel))
        for (d = 0; d < NDROPS; d++)    /* (each DROP's break, dm_put's [2]) */
            if (s->dm[d][2] && s->dm[d][2] != DBRK_NEXT)
                s->dm[d][2] = (uint8_t)(sy_map(s->dm[d][2] - 1u, dir) + 1u);
}
static uint16_t sy_keys_row[BRK_KEYS];   /* (the keys playing, for the breaks with none of their own) */
static void sy_keys_now(const settings_t *st)
{
    uint32_t k;
    for (k = 0; k < BRK_KEYS; k++)
        sy_keys_row[k] = wk_pack(&st->keys[k]);
}
static void sy_kx2_to_fm1(kx2_fm1_t *o)
{
    uint32_t f, v;
    memset(o, 0, sizeof *o);
    memcpy(o, &kx2, offsetof(kx2_t, sp));
    o->ch_tag = kx2.ch_tag;
    memcpy(o->chaos, kx2.chaos, sizeof o->chaos);
    o->ch_sw = kx2.ch_sw;
    o->selrec = kx2.selrec;
    for (f = 0; f < FM1_WK_NB; f++) {
        for (v = 0; v < sy_nv && !(f < sy_nf && sy_same(sy_vslot(v), sy_fslot(f))); v++)
            ;
        if (v < sy_nv) {                 /* the same break on both: its keys and its split */
            uint32_t s = kx2.sp[v], b = s & SP_BMASK;
            memcpy(o->wk[f], kx2.wk[v], sizeof o->wk[f]);
            memcpy(o->wks[f], kx2.wks[v], sizeof o->wks[f]);
            if (s & SP_ON || b)
                o->sp[f] = (uint8_t)((s & SP_ON ? 0x80u : 0u) | (b ? sy_to_fm1(b - 1u) + 1u : 0u) |
                                     (s >> SP_BARS & 3u) << 5);
        } else {                         /* (a break the VST does not have: the keys playing) */
            memcpy(o->wk[f], sy_keys_row, sizeof o->wk[f]);
            memcpy(o->wks[f], sy_keys_row, sizeof o->wks[f]);
        }
    }
    memcpy(o->wk[FM1_WK_NB], kx2.wk[WK_SPLIT], sizeof o->wk[FM1_WK_NB]);
    o->sp_tag = SP_TAG2;
}
static void sy_kx2_from_fm1(const kx2_fm1_t *o, uint32_t n)
{
    uint32_t f, v;
    kx2_defaults();
    memcpy(&kx2, o, n < offsetof(kx2_t, sp) ? n : offsetof(kx2_t, sp));
    if (n < sizeof *o)                   /* (an older FM-1's: as app_init reads one) */
        return;
    kx2.ch_tag = o->ch_tag;
    memcpy(kx2.chaos, o->chaos, sizeof o->chaos);
    kx2.ch_sw = o->ch_sw;
    kx2.selrec = o->selrec;
    kx2.sp_tag = SP_TAG2;
    for (v = 0; v < WK_NB; v++) {
        for (f = 0; f < sy_nf && !(v < sy_nv && sy_same(sy_vslot(v), sy_fslot(f))); f++)
            ;
        if (f < sy_nf) {
            uint32_t s = o->sp[f], b = s & 31u;
            memcpy(kx2.wk[v], o->wk[f], sizeof kx2.wk[v]);
            memcpy(kx2.wks[v], o->wks[f], sizeof kx2.wks[v]);
            kx2.sp[v] = (sp_t)((s & 0x80u ? SP_ON : 0u) | (b ? sy_to_vst(b - 1u) + 1u : 0u) |
                               (s >> 5 & 3u) << SP_BARS);
        } else {
            memcpy(kx2.wk[v], sy_keys_row, sizeof kx2.wk[v]);
            memcpy(kx2.wks[v], sy_keys_row, sizeof kx2.wks[v]);
            kx2.sp[v] = 0;
        }
    }
    memcpy(kx2.wk[WK_SPLIT], o->wk[FM1_WK_NB], sizeof kx2.wk[WK_SPLIT]);
}

static const uint16_t SY_OBJS[] = APP_BACKUP_OBJS;
#define SY_N (sizeof SY_OBJS / sizeof SY_OBJS[0])
static uint8_t sy_buf[SY_N][PLAT_STORE_MAX];
static int32_t sy_len[SY_N];
static int sy_at(uint32_t obj)
{
    uint32_t i;
    for (i = 0; i < SY_N; i++)
        if (SY_OBJS[i] == obj)
            return (int)i;
    return -1;
}

/* the VST's state as an FM-1 backup image (out: room for it, 16 + 20 x (8 + 3840) + 4 bytes); its length, 0 failed.
 * report: what was translated, in words */
uint32_t lb_fm1_export(uint8_t *out, uint32_t cap, char *report, uint32_t rcap)
{
    uint32_t i, at;
    int ps = sy_at(OBJ_PAT), ss = sy_at(OBJ_SETTINGS), st = sy_at(OBJ_STEP);
    sy_rep = report, sy_repn = rcap, report[0] = 0;
    store_all();                         /* (everything as it is now) */
    sy_maps();
    for (i = 0; i < SY_N; i++) {
        int z = plat_store_size(SY_OBJS[i]);
        sy_len[i] = z > 0 && plat_store_load(SY_OBJS[i], sy_buf[i], (uint32_t)z) == z ? z : 0;
    }
    if (ss >= 0)
        sy_keys_now((const settings_t *)sy_buf[ss]);
    if (ss >= 0)
        sy_settings(sy_buf[ss], (uint32_t)sy_len[ss], 0);
    if (st >= 0 && ps >= 0)
        sy_steps(sy_buf[st], (uint32_t)sy_len[st], sy_buf[ps], (uint32_t)sy_len[ps], 0);
    i = (uint32_t)sy_at(OBJ_KX2);
    sy_kx2_to_fm1((kx2_fm1_t *)sy_buf[i]);
    sy_len[i] = (int32_t)sizeof(kx2_fm1_t);
    if (cap < 20u)
        return 0;
    memcpy(out, "LBRK", 4);
    sy_w16(out + 4, APP_BACKUP_VERSION);
    sy_w16(out + 6, (uint32_t)SY_N);
    sy_w32(out + 8, 0);
    sy_w32(out + 12, sy_crc(out, 12, 0));
    at = 16;
    for (i = 0; i < SY_N; i++) {
        uint32_t n = (uint32_t)sy_len[i];
        if (at + 8u + n + 4u > cap)
            return 0;
        sy_w16(out + at, SY_OBJS[i]);
        sy_w16(out + at + 2, n);
        sy_w32(out + at + 4, sy_crc(sy_buf[i], n, 0));
        memcpy(out + at + 8, sy_buf[i], n);
        at += 8u + n;
    }
    sy_w32(out + at, sy_crc(out, at, 0));
    sy_report_export();
    return at + 4u;
}

/* an FM-1 backup image into the VST's store (then the plugin boots a fresh core with lb_flash_get's image): 0 done,
 * else 1 not an LBRK backup, 2 damaged */
int lb_fm1_import(const uint8_t *img, uint32_t n, char *report, uint32_t rcap)
{
    uint32_t i, at, cnt;
    int ps, ss, st;
    sy_rep = report, sy_repn = rcap, report[0] = 0;
    if (n < 20u || memcmp(img, "LBRK", 4) || sy_r32(img + 12) != sy_crc(img, 12, 0))
        return 1;
    if (sy_r32(img + n - 4u) != sy_crc(img, n - 4u, 0))
        return 2;
    cnt = sy_r16(img + 6);
    memset(sy_len, 0, sizeof sy_len);
    at = 16;
    for (i = 0; i < cnt; i++) {
        uint32_t obj, len;
        int k;
        if (at + 8u > n - 4u)
            return 2;
        obj = sy_r16(img + at), len = sy_r16(img + at + 2);
        if (len > PLAT_STORE_MAX || at + 8u + len > n - 4u || sy_crc(img + at + 8, len, 0) != sy_r32(img + at + 4))
            return 2;
        if ((k = sy_at(obj)) >= 0) {
            memcpy(sy_buf[k], img + at + 8, len);
            sy_len[k] = (int32_t)len;
        }
        at += 8u + len;
    }
    sy_maps();
    ps = sy_at(OBJ_PAT), ss = sy_at(OBJ_SETTINGS), st = sy_at(OBJ_STEP);
    if (ss >= 0 && sy_len[ss] > (int32_t)offsetof(settings_t, keys) + (int32_t)sizeof set.keys)
        sy_keys_now((const settings_t *)sy_buf[ss]);
    if (ss >= 0)
        sy_settings(sy_buf[ss], (uint32_t)sy_len[ss], 1);
    if (st >= 0 && ps >= 0)
        sy_steps(sy_buf[st], (uint32_t)sy_len[st], sy_buf[ps], (uint32_t)sy_len[ps], 1);
    for (i = 0; i < SY_N; i++) {
        if (SY_OBJS[i] == OBJ_KX2) {
            sy_kx2_from_fm1((const kx2_fm1_t *)sy_buf[i], (uint32_t)sy_len[i]);
            kx2_save();
        } else if (sy_len[i] > 0)
            plat_store_save(SY_OBJS[i], sy_buf[i], (uint32_t)sy_len[i]);
    }
    sy_say("FROM THE FM-1: EVERYTHING IN; ITS OWN BREAKS (IF ANY) AS THE VST'S FIRST.");
    return 0;
}

/* ---- the pick (FLEXOR, 24 Nov 2026: "exporter ou importer des choses, patterns ou tracks ou kicks"): some kicks,
 * patterns and tracks between the VST and an FM-1 backup, everything else on the other side left byte for byte as it
 * was (its own breaks, its settings, its other kicks and patterns). Both sides as their saved objects (the VST's after
 * store_all), the same layouts (the firmware's structs; kx2 to its kicks' part, which both share) ---- */
static uint8_t sy_vbuf[SY_N][PLAT_STORE_MAX];   /* the VST's objects (sy_buf: the FM-1's) */
static int32_t sy_vlen[SY_N];
static uint8_t *sy_ob(int fm1, uint32_t id, uint32_t need)
{
    int k = sy_at(id);
    if (k < 0)
        return 0;
    if ((fm1 ? sy_len[k] : sy_vlen[k]) < (int32_t)need)
        return 0;
    return fm1 ? sy_buf[k] : sy_vbuf[k];
}
static void sy_vload(void)               /* the VST's state as its objects */
{
    uint32_t i;
    store_all();
    for (i = 0; i < SY_N; i++) {
        int z = plat_store_size(SY_OBJS[i]);
        sy_vlen[i] = z > 0 && plat_store_load(SY_OBJS[i], sy_vbuf[i], (uint32_t)z) == z ? z : 0;
    }
}
static int sy_parse(const uint8_t *img, uint32_t n)   /* an FM-1 backup into sy_buf / sy_len: 0, 1 not one, 2 damaged */
{
    uint32_t i, at, cnt;
    if (n < 20u || memcmp(img, "LBRK", 4) || sy_r32(img + 12) != sy_crc(img, 12, 0))
        return 1;
    if (sy_r32(img + n - 4u) != sy_crc(img, n - 4u, 0))
        return 2;
    cnt = sy_r16(img + 6);
    memset(sy_len, 0, sizeof sy_len);
    at = 16;
    for (i = 0; i < cnt; i++) {
        uint32_t obj, len;
        int k;
        if (at + 8u > n - 4u)
            return 2;
        obj = sy_r16(img + at), len = sy_r16(img + at + 2);
        if (len > PLAT_STORE_MAX || at + 8u + len > n - 4u || sy_crc(img + at + 8, len, 0) != sy_r32(img + at + 4))
            return 2;
        if ((k = sy_at(obj)) >= 0) {
            memcpy(sy_buf[k], img + at + 8, len);
            sy_len[k] = (int32_t)len;
        }
        at += 8u + len;
    }
    return 0;
}
static uint32_t sy_build(uint8_t *out, uint32_t cap)   /* sy_buf as an FM-1 backup image (every object, in order, an
                                          * empty one as none: the FM-1 checks them all); its length, 0 no room */
{
    uint32_t i, at, cnt = SY_N;
    if (cap < 20u)
        return 0;
    memcpy(out, "LBRK", 4);
    sy_w16(out + 4, APP_BACKUP_VERSION);
    sy_w16(out + 6, cnt);
    sy_w32(out + 8, 0);
    sy_w32(out + 12, sy_crc(out, 12, 0));
    at = 16;
    for (i = 0; i < SY_N; i++) {
        uint32_t n = sy_len[i] > 0 ? (uint32_t)sy_len[i] : 0u;
        if (at + 8u + n + 4u > cap)
            return 0;
        sy_w16(out + at, SY_OBJS[i]);
        sy_w16(out + at + 2, n);
        sy_w32(out + at + 4, sy_crc(sy_buf[i], n, 0));
        memcpy(out + at + 8, sy_buf[i], n);
        at += 8u + n;
    }
    sy_w32(out + at, sy_crc(out, at, 0));
    return at + 4u;
}

/* a kick memory whole: the kick, its SYNTH, its knobs, its LAB, its FX section, its TOK and GATE */
typedef struct {
    kick_t k;
    int8_t ks[KS_N];
    uint8_t kx[KX_N], kxh;
    kick_lab_t lab;
    kfxp_t fx[BRK_KFX_SLOTS];
    uint16_t eq[8];
    kick_x2_t x2;
} sy_kick_t;
typedef struct {
    settings_t *S;
    steps_t *ST;
    km2_t *K2;
    klab_obj_t *KL;
    kfxm_t *KF;
    kick_x2_t *X;
} sy_kobjs_t;
static int sy_kobjs(int fm1, sy_kobjs_t *o)
{
    uint8_t *x = sy_ob(fm1, OBJ_KX2, offsetof(kx2_t, m) + sizeof kx2.m);
    o->S = (settings_t *)sy_ob(fm1, OBJ_SETTINGS, sizeof(settings_t));
    o->ST = (steps_t *)sy_ob(fm1, OBJ_STEP, offsetof(steps_t, kpre));
    o->K2 = (km2_t *)sy_ob(fm1, OBJ_KMEM2, sizeof(km2_t));
    o->KL = (klab_obj_t *)sy_ob(fm1, OBJ_KLAB, sizeof(klab_obj_t));
    o->KF = (kfxm_t *)sy_ob(fm1, OBJ_KFXM, sizeof(kfxm_t));
    o->X = x ? (kick_x2_t *)(x + offsetof(kx2_t, m)) : 0;
    return o->S && o->ST && o->K2 && o->KL && o->KF && o->X;
}
static float *sy_kf(settings_t *S, uint32_t j, uint32_t i)   /* kfx_f's setting, in that settings' copy */
{
    return (float *)(void *)((uint8_t *)S + ((uint8_t *)kfx_f(j, i) - (uint8_t *)&set));
}
static void sy_kick_get(const sy_kobjs_t *o, uint32_t n, sy_kick_t *c)
{
    settings_t *S = o->S;
    uint32_t j, i, live = n == S->kslot % KICK_MEMS, a = n < KMEM_A;
    c->k = live ? S->kick : a ? S->kmem[n] : o->K2->kmem[n - KMEM_A];
    memcpy(c->ks, live ? S->ksyn : a ? S->ksmem[n] : o->K2->ksmem[n - KMEM_A], KS_N);
    if (live) {                          /* (the kick playing: its knobs in the settings, kx_put's order) */
        c->kx[0] = S->selk_asg, c->kx[1] = S->selk_asg2, c->kx[2] = S->csub;
        memcpy(c->kx + 3, S->kbass, 4);
        c->kx[7] = S->kmorph, c->kx[8] = S->kmorph_to;
        c->kxh = S->selk_hi;
        c->lab = o->KL->cur;
        for (j = 0; j < BRK_KFX_SLOTS; j++) {   /* (kfx_put's) */
            c->fx[j].b[0] = S->kfx[j].type, c->fx[j].b[1] = S->kfx[j].on;
            c->fx[j].b[2] = S->kfxx[j].type, c->fx[j].b[3] = S->kmbc[j].solo;
            for (i = 0; i < KFX_NF; i++)
                c->fx[j].v[i] = kfx_q(*sy_kf(S, j, i));
        }
        for (i = 0; i < 8u; i++)
            c->eq[i] = kfx_q(S->keq[i]);
    } else {
        memcpy(c->kx, a ? o->ST->kx[n] : o->K2->kx[n - KMEM_A], KX_N);
        c->kxh = a ? o->ST->kxh[n] : o->K2->kxh[n - KMEM_A];
        c->lab = a ? o->KL->mem[n] : o->K2->lab[n - KMEM_A];
        memcpy(c->fx, o->KF->k[n].s, sizeof c->fx);
        memcpy(c->eq, o->KF->k[n].eq, sizeof c->eq);
    }
    c->x2 = o->X[n];
}
static void sy_kick_put(const sy_kobjs_t *o, uint32_t n, const sy_kick_t *c)
{
    settings_t *S = o->S;
    uint32_t j, i, a = n < KMEM_A;
    if (a) {
        S->kmem[n] = c->k;
        memcpy(S->ksmem[n], c->ks, KS_N);
        memcpy(o->ST->kx[n], c->kx, KX_N);
        o->ST->kxh[n] = c->kxh;
        o->KL->mem[n] = c->lab;
    } else {
        o->K2->kmem[n - KMEM_A] = c->k;
        memcpy(o->K2->ksmem[n - KMEM_A], c->ks, KS_N);
        memcpy(o->K2->kx[n - KMEM_A], c->kx, KX_N);
        o->K2->kxh[n - KMEM_A] = c->kxh;
        o->K2->lab[n - KMEM_A] = c->lab;
    }
    memcpy(o->KF->k[n].s, c->fx, sizeof c->fx);
    memcpy(o->KF->k[n].eq, c->eq, sizeof c->eq);
    o->X[n] = c->x2;
    if (n == S->kslot % KICK_MEMS) {     /* the kick playing there: it becomes this one (its volume kept: kick_mem_go) */
        float vol = S->kick.volume;
        S->kick = c->k;
        S->kick.volume = vol;
        memcpy(S->ksyn, c->ks, KS_N);
        S->selk_asg = c->kx[0], S->selk_asg2 = c->kx[1], S->csub = c->kx[2];
        memcpy(S->kbass, c->kx + 3, 4);
        S->kmorph = c->kx[7], S->kmorph_to = c->kx[8];
        S->selk_hi = c->kxh;
        o->KL->cur = c->lab;
        for (j = 0; j < BRK_KFX_SLOTS; j++) {   /* (kfx_get's) */
            S->kfx[j].type = c->fx[j].b[0] < BRK_NFXT ? c->fx[j].b[0] : FXT_EMPTY;
            S->kfx[j].on = c->fx[j].b[1];
            S->kfxx[j].type = c->fx[j].b[2];
            S->kmbc[j].solo = c->fx[j].b[3];
            for (i = 0; i < KFX_NF; i++)
                *sy_kf(S, j, i) = (float)c->fx[j].v[i] * (1.0f / 65535.0f);
        }
        for (i = 0; i < 8u; i++)
            S->keq[i] = (float)c->eq[i] * (1.0f / 65535.0f);
    }
}
static const char *sy_kick_name(const sy_kick_t *c, uint32_t n)
{
    uint32_t p = c->lab.pre ? (uint32_t)(c->lab.pre - 1u) : n % KICK_PRESETS_MEM;
    return p < KICK_PRESETS ? KICK_PRESET_NAME[p] : "";
}

/* a pattern whole: its steps, the kick's notes past its 38th step, the drums' HITS, each drum step's own, the drums'
 * bars 3-4. Where: the group in RAM's in the patterns' and steps' objects (and its own, when saved), another group's
 * in its own three (none saved: its 16 empty), the bars 3-4 in OBJ_DX */
typedef struct {
    pat_t p;
    uint8_t kn[13], hits[NDRUMS_SAVED], b[STEPB_N];
    uint16_t dx[6];
    uint8_t has_dx;
} sy_pat_t;
static void sy_pat_blank(sy_pat_t *c)    /* (grp_blank's) */
{
    uint32_t k;
    memset(c, 0, sizeof *c);
    for (k = 0; k < PAT_STEPS; k++)
        c->p.st[k][0] = 1u;
    c->p.st[6][3] = 27u;
}
static int sy_pat_get(int fm1, uint32_t P, sy_pat_t *c)   /* 1: found (0: never saved there, an empty one) */
{
    const settings_t *S = (const settings_t *)sy_ob(fm1, OBJ_SETTINGS, sizeof(settings_t));
    uint32_t g = P / NPAT % NGRP, i = P % NPAT;
    uint8_t *pa = sy_ob(fm1, OBJ_PAT, sizeof(pats_t)), *st = sy_ob(fm1, OBJ_STEP, offsetof(steps_t, tb));
    uint8_t *pg = sy_ob(fm1, OBJ_PG0 + g, sizeof pats.p), *kg = sy_ob(fm1, OBJ_KG0 + g, KH_SIZE);
    uint8_t *sg = sy_ob(fm1, OBJ_SG0 + g, sizeof steps.b), *dxo = sy_ob(fm1, OBJ_DX, sizeof(dx_t));
    int found = 0;
    sy_pat_blank(c);
    if (S && S->pgrp % NGRP == g && pa && st) {   /* (the group in RAM: the patterns' and steps' objects) */
        const pats_t *A = (const pats_t *)pa;
        c->p = A->p[i];
        memcpy(c->kn, A->kn[i], 13);
        memcpy(c->hits, A->hits[i], NDRUMS_SAVED);
        memcpy(c->b, ((const steps_t *)st)->b[i], STEPB_N);
        found = 1;
    } else if (pg && kg && sg) {
        c->p = ((const pat_t *)pg)[i];
        memcpy(c->kn, kg + i * 13u, 13);
        memcpy(c->hits, kg + sizeof pats.kn + i * NDRUMS_SAVED, NDRUMS_SAVED);
        memcpy(c->b, sg + i * STEPB_N, STEPB_N);
        found = 1;
    }
    if (dxo && ((const dx_t *)dxo)->magic == DX_MAGIC) {
        memcpy(c->dx, ((const dx_t *)dxo)->b[g][i], sizeof c->dx);
        c->has_dx = 1;
    }
    return found;
}
static int sy_grow(int fm1, uint32_t id, uint32_t n)   /* an object the other side never saved: made, blank (1 done) */
{
    int k = sy_at(id);
    if (k < 0)
        return 0;
    if (fm1) {
        if (!sy_len[k]) memset(sy_buf[k], 0, n), sy_len[k] = (int32_t)n;
        return sy_len[k] >= (int32_t)n;
    }
    if (!sy_vlen[k]) memset(sy_vbuf[k], 0, n), sy_vlen[k] = (int32_t)n;
    return sy_vlen[k] >= (int32_t)n;
}
static void sy_pat_put(int fm1, uint32_t P, const sy_pat_t *c)
{
    const settings_t *S = (const settings_t *)sy_ob(fm1, OBJ_SETTINGS, sizeof(settings_t));
    uint32_t g = P / NPAT % NGRP, i = P % NPAT, j;
    uint8_t *pa = sy_ob(fm1, OBJ_PAT, sizeof(pats_t)), *st = sy_ob(fm1, OBJ_STEP, offsetof(steps_t, tb)), *pg, *kg, *sg, *dxo;
    if (S && S->pgrp % NGRP == g && pa && st) {
        pats_t *A = (pats_t *)pa;
        A->p[i] = c->p;
        memcpy(A->kn[i], c->kn, 13);
        memcpy(A->hits[i], c->hits, NDRUMS_SAVED);
        memcpy(((steps_t *)st)->b[i], c->b, STEPB_N);
    }
    if (!(S && S->pgrp % NGRP == g) && !sy_ob(fm1, OBJ_PG0 + g, sizeof pats.p)) {   /* (a group never saved there:
                                          * its 16 empty, then this one) */
        sy_pat_t e;
        sy_pat_blank(&e);
        sy_grow(fm1, OBJ_PG0 + g, sizeof pats.p);
        sy_grow(fm1, OBJ_KG0 + g, KH_SIZE);
        sy_grow(fm1, OBJ_SG0 + g, sizeof steps.b);
        pg = sy_ob(fm1, OBJ_PG0 + g, sizeof pats.p);
        for (j = 0; pg && j < NPAT; j++)
            ((pat_t *)pg)[j] = e.p;
    }
    pg = sy_ob(fm1, OBJ_PG0 + g, sizeof pats.p);
    kg = sy_ob(fm1, OBJ_KG0 + g, KH_SIZE);
    sg = sy_ob(fm1, OBJ_SG0 + g, sizeof steps.b);
    if (pg && kg && sg) {                /* (its own, the group's) */
        ((pat_t *)pg)[i] = c->p;
        memcpy(kg + i * 13u, c->kn, 13);
        memcpy(kg + sizeof pats.kn + i * NDRUMS_SAVED, c->hits, NDRUMS_SAVED);
        memcpy(sg + i * STEPB_N, c->b, STEPB_N);
    }
    dxo = sy_ob(fm1, OBJ_DX, sizeof(dx_t));
    if (dxo && ((dx_t *)dxo)->magic == DX_MAGIC && c->has_dx)
        memcpy(((dx_t *)dxo)->b[g][i], c->dx, sizeof c->dx);
}
static int sy_pat_used(const sy_pat_t *c)   /* (notes on it: the drums' bars 1-2, the break's steps not LOOP) */
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        if (c->p.lane[k])
            return 1;
    for (k = 0; k < PAT_STEPS; k++)
        if (c->p.st[k][0] != 1u || c->p.st[k][1])
            return 1;
    return 0;
}

/* a track: its items, their loops, its end; each item's break (translated), its kick */
typedef struct {
    track_t t;
    uint8_t tb[TRK_ITEMS], tk[TRK_ITEMS];
} sy_trk_t;
static int sy_trk_get(int fm1, uint32_t t, sy_trk_t *c)
{
    uint8_t *pa = sy_ob(fm1, OBJ_PAT, offsetof(pats_t, mem)), *st = sy_ob(fm1, OBJ_STEP, offsetof(steps_t, kxh));
    if (!pa || !st)
        return 0;
    c->t = ((const pats_t *)pa)->trk[t % NTRACKS];
    memcpy(c->tb, ((const steps_t *)st)->tb[t % NTRACKS], TRK_ITEMS);
    memcpy(c->tk, ((const steps_t *)st)->tk[t % NTRACKS], TRK_ITEMS);
    return 1;
}
static void sy_trk_put(int fm1, uint32_t t, sy_trk_t *c)
{
    uint8_t *pa = sy_ob(fm1, OBJ_PAT, offsetof(pats_t, mem)), *st = sy_ob(fm1, OBJ_STEP, offsetof(steps_t, kxh));
    uint32_t i;
    if (!pa || !st)
        return;
    for (i = 0; i < TRK_ITEMS; i++) {    /* (a pattern item's break: the other side's number) */
        uint32_t it = c->t.it[i];
        if (it != TI_END && it != TI_DROP && it != TI_GAP && c->tb[i])
            c->tb[i] = (uint8_t)(sy_map(c->tb[i] - 1u, fm1 ? 0 : 1) + 1u);
    }
    ((pats_t *)pa)->trk[t % NTRACKS] = c->t;
    memcpy(((steps_t *)st)->tb[t % NTRACKS], c->tb, TRK_ITEMS);
    memcpy(((steps_t *)st)->tk[t % NTRACKS], c->tk, TRK_ITEMS);
}
static int sy_trk_len(const sy_trk_t *c)
{
    int i = 0;
    while (i < (int)TRK_ITEMS && c->t.it[i] != TI_END)
        i++;
    return i;
}

/* one pick: from one side to the other (fm1: 1 into the FM-1's, 0 into the VST's) */
static int sy_pick(const lb_pick_t *p, int fm1)
{
    if (p->kind == LB_PICK_KICK) {
        sy_kobjs_t a, b;
        sy_kick_t c;
        if (p->from >= KICK_MEMS || p->to >= KICK_MEMS || !sy_kobjs(!fm1, &a) || !sy_kobjs(fm1, &b))
            return 0;
        sy_kick_get(&a, p->from, &c);
        sy_kick_put(&b, p->to, &c);
        return 1;
    }
    if (p->kind == LB_PICK_PATTERN) {
        sy_pat_t c;
        if (p->from >= NPAT_ALL || p->to >= NPAT_ALL)
            return 0;
        sy_pat_get(!fm1, p->from, &c);
        sy_pat_put(fm1, p->to, &c);
        return 1;
    }
    if (p->kind == LB_PICK_TRACK) {
        sy_trk_t c;
        if (p->from >= NTRACKS || p->to >= NTRACKS || !sy_trk_get(!fm1, p->from, &c))
            return 0;
        sy_trk_put(fm1, p->to, &c);
        return 1;
    }
    return 0;
}

/* what a side holds, for choosing: img an FM-1 backup, or 0 the VST's own */
int lb_fm1_list(const uint8_t *img, uint32_t n, lb_list_t *o)
{
    sy_kobjs_t k;
    uint32_t i;
    int side = img != 0;
    memset(o, 0, sizeof *o);
    if (img) {
        int r = sy_parse(img, n);
        if (r)
            return r;
    } else
        sy_vload();
    if (sy_kobjs(side, &k))
        for (i = 0; i < KICK_MEMS; i++) {
            sy_kick_t c;
            sy_kick_get(&k, i, &c);
            snprintf(o->kick[i], sizeof o->kick[i], "%s", sy_kick_name(&c, i));
            o->kick_live = (int)(k.S->kslot % KICK_MEMS);
        }
    for (i = 0; i < NPAT_ALL; i++) {
        sy_pat_t c;
        o->pat[i] = (uint8_t)(sy_pat_get(side, i, &c) ? 1u + (uint32_t)sy_pat_used(&c) : 0u);
    }
    for (i = 0; i < NTRACKS; i++) {
        sy_trk_t c;
        o->trk[i] = (uint8_t)(sy_trk_get(side, i, &c) ? sy_trk_len(&c) : 0);
    }
    return 0;
}

/* VST -> FM-1: the picks written into an FM-1 backup (fetched just before), the rest of it as it was: the new image
 * (to send back with a restore); its length, 0 failed (report: why) */
uint32_t lb_fm1_put(const uint8_t *fm1, uint32_t n, const lb_pick_t *p, int np, uint8_t *out, uint32_t cap, char *report,
                    uint32_t rcap)
{
    int i, done = 0;
    char t[48];
    sy_rep = report, sy_repn = rcap, report[0] = 0;
    if (sy_parse(fm1, n)) {
        sy_say("THE FM-1'S BACKUP: DAMAGED OR NOT ONE.");
        return 0;
    }
    sy_vload();
    sy_maps();
    for (i = 0; i < np; i++)
        done += sy_pick(&p[i], 1);
    snprintf(t, sizeof t, "%d SENT, THE REST OF THE FM-1 AS IT WAS.", done);
    sy_say(t);
    return sy_build(out, cap);
}

/* FM-1 -> VST: the picks from an FM-1 backup into the VST's store (then the plugin boots a fresh core from
 * lb_flash_get's image, as after lb_fm1_import): the picks taken, -1 not a backup */
int lb_fm1_take(const uint8_t *fm1, uint32_t n, const lb_pick_t *p, int np, char *report, uint32_t rcap)
{
    int i, done = 0;
    uint32_t k;
    char t[48];
    sy_rep = report, sy_repn = rcap, report[0] = 0;
    if (sy_parse(fm1, n))
        return -1;
    sy_vload();
    sy_maps();
    for (i = 0; i < np; i++)
        done += sy_pick(&p[i], 0);
    for (k = 0; k < SY_N; k++) {
        if (SY_OBJS[k] == OBJ_KX2) {     /* (the kicks' part of kx2, the VST's own layout past it: in place, saved) */
            if (sy_vlen[k] >= (int32_t)(offsetof(kx2_t, m) + sizeof kx2.m))
                memcpy(kx2.m, sy_vbuf[k] + offsetof(kx2_t, m), sizeof kx2.m);
            kx2_save();
        } else if (sy_vlen[k] > 0)
            plat_store_save(SY_OBJS[k], sy_vbuf[k], (uint32_t)sy_vlen[k]);
    }
    snprintf(t, sizeof t, "%d TAKEN FROM THE FM-1.", done);
    sy_say(t);
    return done;
}

/* ---- one kick as a blob (FLEXOR: "écouter sur le PC les kicks de la machine ... importer des kicks de la machine, les
 * nommer et les ranger"): the plugin keeps them in its library, each a file; "LBK1", its size, the kick whole ---- */
#define SY_KBLOB_MAGIC 0x314B424Cu       /* "LBK1" */
uint32_t lb_kick_size(void) { return 8u + (uint32_t)sizeof(sy_kick_t); }
static int sy_kblob_ok(const uint8_t *b) { return sy_r32(b) == SY_KBLOB_MAGIC && sy_r32(b + 4) == sizeof(sy_kick_t); }
/* kick memory k (0..19) of an FM-1 backup (img) or the VST (img 0) as a blob: 1 done */
int lb_kick_read(const uint8_t *img, uint32_t n, int k, uint8_t *blob)
{
    sy_kobjs_t o;
    sy_kick_t c;
    int side = img != 0;
    if (k < 0 || k >= (int)KICK_MEMS)
        return 0;
    if (img ? sy_parse(img, n) != 0 : (sy_vload(), 0))
        return 0;
    if (!sy_kobjs(side, &o))
        return 0;
    sy_kick_get(&o, (uint32_t)k, &c);
    sy_w32(blob, SY_KBLOB_MAGIC);
    sy_w32(blob + 4, sizeof c);
    memcpy(blob + 8, &c, sizeof c);
    return 1;
}
/* blobs into an FM-1 backup's kick memories (to[i] 0..19): the new image's length, 0 failed */
uint32_t lb_fm1_put_kicks(const uint8_t *fm1, uint32_t n, const uint8_t *blobs, const uint8_t *to, int nk, uint8_t *out,
                          uint32_t cap)
{
    sy_kobjs_t o;
    int i;
    if (sy_parse(fm1, n) || !sy_kobjs(1, &o))
        return 0;
    for (i = 0; i < nk; i++) {
        const uint8_t *b = blobs + (uint32_t)i * lb_kick_size();
        sy_kick_t c;
        if (!sy_kblob_ok(b) || to[i] >= KICK_MEMS)
            continue;
        memcpy(&c, b + 8, sizeof c);
        sy_kick_put(&o, to[i], &c);
    }
    return sy_build(out, cap);
}
/* blobs into the VST's kick memories, its store (then a fresh core, as after lb_fm1_take): how many */
int lb_vst_put_kicks(const uint8_t *blobs, const uint8_t *to, int nk)
{
    sy_kobjs_t o;
    int i, done = 0;
    uint32_t k;
    sy_vload();
    if (!sy_kobjs(0, &o))
        return 0;
    for (i = 0; i < nk; i++) {
        const uint8_t *b = blobs + (uint32_t)i * lb_kick_size();
        sy_kick_t c;
        if (!sy_kblob_ok(b) || to[i] >= KICK_MEMS)
            continue;
        memcpy(&c, b + 8, sizeof c);
        sy_kick_put(&o, to[i], &c);
        done++;
    }
    for (k = 0; k < SY_N; k++) {
        if (SY_OBJS[k] == OBJ_KX2) {
            memcpy(kx2.m, sy_vbuf[k] + offsetof(kx2_t, m), sizeof kx2.m);
            kx2_save();
        } else if (sy_vlen[k] > 0)
            plat_store_save(SY_OBJS[k], sy_vbuf[k], (uint32_t)sy_vlen[k]);
    }
    return done;
}
