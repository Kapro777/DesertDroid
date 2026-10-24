/* DesertDroid — rastreador de callers via overrides.
 *
 * Registra um override que LOGA quem chamou (g_cur_func do caller)
 * e depois chama o ORIGINAL via recomp_lookup (dispatch normal).
 * Usado para descobrir quem chama os inits do MM/heap que nao tem
 * callers estaticos (metodos virtuais com dispatch dinamico).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include "win32hle.h"

extern uint32_t g_cur_func;
extern uint32_t hle_call_guest(uint32_t va, int nargs, const uint32_t *args);

static void dd_tracer_00AA20E0(void)
{
    fprintf(stderr, "[TRACER] 00AA20E0 (heap init) chamado por sub_%08X!\n",
            g_cur_func);
    hle_call_guest(0x00AA20E0u, 0, NULL);   /* chamar o original */
}

static void dd_tracer_00AA2020(void)
{
    fprintf(stderr, "[TRACER] 00AA2020 (MM init) chamado por sub_%08X!\n",
            g_cur_func);
    hle_call_guest(0x00AA2020u, 0, NULL);
}

static void dd_tracer_00AA211C(void)
{
    fprintf(stderr, "[TRACER] 00AA211C (heap global write) chamado por sub_%08X!\n",
            g_cur_func);
    hle_call_guest(0x00AA211Cu, 0, NULL);
}

void dd_tracers_register(void)
{
    dd_overrides_add(0x00AA20E0u, dd_tracer_00AA20E0);
    dd_overrides_add(0x00AA2020u, dd_tracer_00AA2020);
    dd_overrides_add(0x00AA211Cu, dd_tracer_00AA211C);
    fprintf(stderr, "[DesertDroid] tracers de init registrados (00AA20E0/2020/211C)\n");
}
