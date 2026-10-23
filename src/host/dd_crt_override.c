/* DesertDroid — overrides cirurgicos do CRT VS2008 (a camada mais rasa).
 *
 * 00ECCD14 (wrapper _initterm): o tail original chama um callback de
 *   terminacao ([0x1271A94]) registrado durante os construtores, que
 *   dispara exit(0) PRE-WinMain (run #30). Override: roda o _initterm
 *   REAL via hle_call_guest e pula o callback.
 * 00ED0F17 (getter de per-thread data): fast path original depende de
 *   ponteiro dinamico ([0xFDF044]) que nao satisfazemos; a fallback
 *   aborta(8) "not enough space for thread data" — mesmo com o ptd JA
 *   EXISTINDO (run #33: slot 1 = 0x50401030 estavel). Override: le o
 *   indice da global do CRT ([0x11C1CC0]) e devolve o valor do slot.
 *
 * Somente codigo DesertDroid aqui; _initterm e codigo LIFTADO, chamado
 * pelo dispatch normal (BYOG — nunca e distribuido).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include "win32hle.h"

extern uint32_t g_eax;
extern void dd_overrides_add(uint32_t va, recomp_func_t fn);
uint32_t dd_tls_slot_value(uint32_t idx);

/* _initterm real = sub_00ECCCF0 (catalogado); args do original:
 *   push 0x10113E8 (__xi_z); push 0x10113C4 (__xi_a); call _initterm */
#define XI_A 0x10113C4u
#define XI_Z 0x10113E8u
#define INITERM_VA 0x00ECCCF0u

static void dd_ovr_00ECCD14(void)
{
    fprintf(stderr, "[DesertDroid] override 00ECCD14: _initterm(xi)...\n");
    {
        const uint32_t args[2] = { XI_A, XI_Z };
        hle_call_guest(INITERM_VA, 2, args);   /* stdcall: push xi_z, xi_a */
    }
    fprintf(stderr, "[DesertDroid] _initterm completou; pulando o callback "
                    "de terminacao [0x1271A94] (exit pre-WinMain)\n");
    g_eax = 0;                                  /* "xor eax,eax; ret"      */
}

static void dd_ovr_00ED0F17(void)
{
    /* O ptd e' o UNICO ponteiro na faixa da arena (0x50000000-0x68000000)
     * nos slots TLS (run #33: slot 1 = 0x50401030 estavel). Scan semantico:
     * nao dependo de qual global do CRT guarda o indice. */
    extern uint32_t dd_tls_slots_snapshot(uint32_t *out, uint32_t n);
    uint32_t vals[64];
    uint32_t n = dd_tls_slots_snapshot(vals, 64);
    uint32_t ptd = 0, slot = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        if (vals[i] >= 0x50000000u && vals[i] < 0x68000000u) {
            ptd = vals[i];
            slot = i;
            break;
        }
    }
    fprintf(stderr, "[DesertDroid] override 00ED0F17: ptd slot %u -> %#x\n",
            slot, ptd);
    g_eax = ptd;                                 /* 0 = quem chama decide  */
}

void dd_crt_overrides_register(void)
{
    dd_overrides_add(0x00ECCD14u, dd_ovr_00ECCD14);
    fprintf(stderr, "[DesertDroid] overrides CRT registrados "
            "(00ECCD14 _initterm)\n");
}
