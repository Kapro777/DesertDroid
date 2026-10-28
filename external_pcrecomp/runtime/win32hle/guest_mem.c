/* guest_mem.c — arena de guest em VA baixo para hosts 64-bit (DesertDroid).
 *
 * Uma reserva MAP_NORESERVE em VA fixo abaixo de 4 GB; alocacoes sao bump
 * alinhado a 16 (paginas anonimas ja vem zeradas). E o equivalente do
 * GuestMemory do nfsu2-android, no tamanho que o boot do jogo precisa.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include "guest_mem.h"

#define ARENA_BASE 0x50000000u   /* longe da imagem (0x400000..) e dos shims (0x7F000000..) */
#define ARENA_SIZE 0x18000000ull /* 384 MB reservados; paginas so viram ao tocar */
#define HDR 16u                  /* header escondido: [0]=tamanho [4]=magic [8]=free-magic */

static volatile uint64_t g_bump;   /* offset do proximo bloco na arena */
static int g_arena_ok;

static int arena_init(void)
{
    void *p = mmap((void *)(uintptr_t)ARENA_BASE, ARENA_SIZE,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
                   -1, 0);
    if (p != (void *)(uintptr_t)ARENA_BASE) {
        fprintf(stderr, "[guest_mem] mmap da arena em %#x falhou (%p)\n",
                ARENA_BASE, p);
        return 0;
    }
    g_bump = 0;
    g_arena_ok = 1;
    fprintf(stderr, "[guest_mem] arena de guest em %#x..%#llx\n",
            ARENA_BASE, (unsigned long long)(ARENA_BASE + ARENA_SIZE));
    return 1;
}

uint32_t guest_alloc(size_t size)
{
    if (!g_arena_ok && !arena_init()) return 0;
    if (size == 0) size = 1;
    size = (size + 15u) & ~(size_t)15u;
    uint64_t off = __sync_fetch_and_add(&g_bump, size + HDR);
    if (off + size + HDR > ARENA_SIZE) return 0;
    uint32_t base = ARENA_BASE + (uint32_t)off;
    *(uint32_t *)(uintptr_t)(base + 0)  = (uint32_t)size;
    *(uint32_t *)(uintptr_t)(base + 4)  = 0xA110C8EDu;
    *(uint32_t *)(uintptr_t)(base + 8)  = 0u;
    return base + HDR;
}

uint32_t guest_realloc(uint32_t old_va, size_t newsize)
{
    if (!old_va) return guest_alloc(newsize);
    if (old_va < ARENA_BASE + HDR || old_va > ARENA_BASE + (uint32_t)ARENA_SIZE)
        return 0;                                  /* nao veio da arena: recusa */
    uint32_t *hdr = (uint32_t *)(uintptr_t)(old_va - HDR);
    if (hdr[1] != 0xA110C8EDu) return 0;
    uint32_t oldsize = hdr[0];
    uint32_t nv = guest_alloc(newsize);
    if (nv)
        memcpy(guest_ptr(nv), guest_ptr(old_va),
               oldsize < newsize ? oldsize : newsize);
    return nv;
}

void guest_free(uint32_t va)
{
    if (va < ARENA_BASE + HDR) return;             /* free() de nao-arena: no-op */
    uint32_t *hdr = (uint32_t *)(uintptr_t)(va - HDR);
    if (hdr[1] == 0xA110C8EDu) hdr[2] = 0xF4EEDBAAu;
}

uint32_t guest_alloc_at(uint32_t va, size_t size)
{
    if (!va) return guest_alloc(size);
    if (size == 0) size = 1;
    size = (size + 0xFFFu) & ~(size_t)0xFFFu;
    void *p = mmap((void *)(uintptr_t)va, size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
                   -1, 0);
    return (p == (void *)(uintptr_t)va) ? va : 0;
}

void *guest_ptr(uint32_t va)
{
    return (void *)(uintptr_t)va;
}

/* Bytes ja consumidos do bump (diagnostico de esgotamento). */
uint64_t guest_mem_used(void)
{
    return g_arena_ok ? g_bump : 0;
}
