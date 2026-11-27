/* LOAF BREAKER VST: the core's whole state (its writable memory: every variable of the firmware, the simulator and
 * the store) kept and put back, so the interface can look ahead without changing anything: every part of a page and
 * every value a setting can take, read through the firmware's own code, then the core exactly as it was (lbcore.c
 * lb_scan, lb_options). Included by lbcore.c. The copy lives on the heap, outside what it copies. */
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/loader.h>
#else
#define _GNU_SOURCE 1
#include <link.h>
#endif

#define SNAP_MAX 8
static struct { uint8_t *at; size_t n; } snap_seg[SNAP_MAX];
static int snap_nseg = -1;
static size_t snap_total;
static uint8_t snap_mark;                /* (an address inside the module's own writable memory) */

#ifdef _WIN32
static void snap_find(void)
{
    HMODULE h = 0;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *s;
    unsigned i;
    snap_nseg = 0;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&snap_mark, &h))
        return;
    dos = (IMAGE_DOS_HEADER *)h;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)h + dos->e_lfanew);
    s = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections && snap_nseg < SNAP_MAX; i++, s++)
        if ((s->Characteristics & IMAGE_SCN_MEM_WRITE) && s->Misc.VirtualSize) {
            snap_seg[snap_nseg].at = (uint8_t *)h + s->VirtualAddress;
            snap_seg[snap_nseg].n = s->Misc.VirtualSize;
            snap_nseg++;
        }
}
#elif defined(__APPLE__)
/* macOS: the image's writable segments (__DATA, __DATA_DIRTY ...), from its own load commands; the *_CONST ones are
 * made read-only once loaded (as ELF's RELRO): never written, so never put back */
static void snap_find(void)
{
    Dl_info di;
    const struct mach_header_64 *h;
    const struct load_command *lc;
    uintptr_t slide = 0;
    unsigned i, pass;
    snap_nseg = 0;
    if (!dladdr(&snap_mark, &di) || !di.dli_fbase)
        return;
    h = (const struct mach_header_64 *)di.dli_fbase;
    if (h->magic != MH_MAGIC_64)
        return;
    for (pass = 0; pass < 2; pass++) {
        lc = (const struct load_command *)(h + 1);
        for (i = 0; i < h->ncmds; i++, lc = (const struct load_command *)((const uint8_t *)lc + lc->cmdsize)) {
            const struct segment_command_64 *sg = (const struct segment_command_64 *)lc;
            size_t nl;
            if (lc->cmd != LC_SEGMENT_64)
                continue;
            if (pass == 0) {
                if (!strcmp(sg->segname, "__TEXT"))
                    slide = (uintptr_t)h - (uintptr_t)sg->vmaddr;
                continue;
            }
            nl = strnlen(sg->segname, 16);
            if (!(sg->initprot & VM_PROT_WRITE) || !sg->vmsize || snap_nseg >= SNAP_MAX)
                continue;
            if (nl >= 6 && !memcmp(sg->segname + nl - 6, "_CONST", 6))
                continue;
            snap_seg[snap_nseg].at = (uint8_t *)(slide + (uintptr_t)sg->vmaddr);
            snap_seg[snap_nseg].n = (size_t)sg->vmsize;
            snap_nseg++;
        }
    }
}
#else
static int snap_cb(struct dl_phdr_info *in, size_t sz, void *u)
{
    uintptr_t me = (uintptr_t)&snap_mark, relro_end = 0;
    int i, mine = 0;
    (void)sz;
    (void)u;
    for (i = 0; i < in->dlpi_phnum; i++) {
        const ElfW(Phdr) *p = &in->dlpi_phdr[i];
        uintptr_t a = in->dlpi_addr + p->p_vaddr;
        if (p->p_type == PT_LOAD && (p->p_flags & PF_W) && me >= a && me < a + p->p_memsz)
            mine = 1;
        if (p->p_type == PT_GNU_RELRO)
            relro_end = a + p->p_memsz;
    }
    if (!mine)
        return 0;
    for (i = 0; i < in->dlpi_phnum && snap_nseg < SNAP_MAX; i++) {
        const ElfW(Phdr) *p = &in->dlpi_phdr[i];
        uintptr_t a = in->dlpi_addr + p->p_vaddr, e = a + p->p_memsz;
        if (p->p_type != PT_LOAD || !(p->p_flags & PF_W))
            continue;
        if (relro_end > a && relro_end < e)
            a = (relro_end + 4095u) & ~(uintptr_t)4095u;   /* (the part made read-only after loading: never written) */
        else if (relro_end >= e)
            continue;
        if (a < e) {
            snap_seg[snap_nseg].at = (uint8_t *)a;
            snap_seg[snap_nseg].n = e - a;
            snap_nseg++;
        }
    }
    return 1;
}
static void snap_find(void)
{
    snap_nseg = 0;
    dl_iterate_phdr(snap_cb, 0);
}
#endif

/* a buffer for one copy of the state (0: none could be found) */
static uint8_t *snap_new(void)
{
    int i;
    if (snap_nseg < 0) {
        snap_find();
        snap_total = 0;
        for (i = 0; i < snap_nseg; i++)
            snap_total += snap_seg[i].n;
    }
    return snap_total ? (uint8_t *)malloc(snap_total) : 0;
}
static void snap_take(uint8_t *b)
{
    int i;
    for (i = 0; i < snap_nseg; i++) {
        memcpy(b, snap_seg[i].at, snap_seg[i].n);
        b += snap_seg[i].n;
    }
}
static void snap_put(const uint8_t *b)
{
    int i;
    for (i = 0; i < snap_nseg; i++) {
        memcpy(snap_seg[i].at, b, snap_seg[i].n);
        b += snap_seg[i].n;
    }
}
