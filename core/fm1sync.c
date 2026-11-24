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
