/* DesertDroid — override do sub_00ECCD14 (wrapper de _initterm do CRT).
 *
 * O original (VA 0x00ECCD14) roda _initterm(xi_a=0x10113C4, xi_z=0x10113E8)
 * — TODOS os construtores estaticos do jogo (Havok, Gamebryo, BGS) — e
 * depois, no tail, chama um callback de terminacao registrado via ponteiro
 * em [0x1271A94] durante os proprios construtores. Esse callback inicia
 * exit(0) PRE-WinMain (run #30: ring + marcadores provam). Este override
 * replica o caminho essencial e pula a parte que mata o boot.
 *
 * Somente codigo DesertDroid aqui; o _initterm real e codigo LIFTADO,
 * chamado pelo dispatch normal (BYOG — nunca e distribuido).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include "win32hle.h"

extern uint32_t g_eax;
extern void dd_overrides_add(uint32_t va, recomp_func_t fn);

/* _initterm real = sub_00ECCCF0 (catalogado); args no original:
 *   push 0x10113E8 (__xi_z); push 0x10113C4 (__xi_a); call _initterm   */
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

void dd_crt_overrides_register(void)
{
    dd_overrides_add(0x00ECCD14u, dd_ovr_00ECCD14);
    fprintf(stderr, "[DesertDroid] override do wrapper _initterm registrado\n");
}
