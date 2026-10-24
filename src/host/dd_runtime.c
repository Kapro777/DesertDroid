/* DesertDroid — runtime bits que o HOST provê ao código gerado.
 *
 * O modelo pcrecomp (registers globais) deixa estes símbolos como
 * responsabilidade do host: o estado SSE (o host_lite define x87 e MMX),
 * o handler de chamadas indiretas não resolvidas, o handler de instruções
 * não liftadas e a intrinsic RDTSC.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "recomp_types.h"
#include "recomp_simd.h"

/* ---- SSE: register file + controle (host_lite cobre x87 em g_st e MMX em g_mm) ---- */
V128     g_xmm[8];
uint32_t g_mxcsr = 0x1F80;   /* exceptions mascaradas, round-to-nearest */

/* ---- RDTSC: relógio monotônico nativo (1.330 sites no FNV, via CRT/ timing) ---- */
uint64_t __rdtsc(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ---- chamada indireta não resolvida em tempo de compilação ----
 * RECOMP_STRICT_ICALL roteia para cá com o VA alvo. A tabela gerada cobre
 * as 119.800 funções catalogadas: se o alvo está nela, a chamada prossegue
 * com a semântica exata do x86; se não, é dado-executado-como-código ou
 * um dos 50 sites não liftados — abortamos nomeando o VA. */
void recomp_unresolved(const char *kind, uint32_t va, uint32_t from)
{
    recomp_func_t fn = recomp_lookup(va);
    if (fn) {
        fn();
        return;
    }
    fprintf(stderr,
            "[DesertDroid] %s: VA nao catalogado 0x%08X (chamado de sub_%08X)\n",
            kind, va, from);
    abort();
}

/* ---- instrução não liftada (50 sites conhecidos; ver lift_stats.json) ---- */
void recomp_unimpl(uint32_t va, const char *what)
{
    fprintf(stderr,
            "[DesertDroid] instrucao nao implementada em 0x%08X: %s\n",
            va, what);
    abort();
}

/* ---- trace (o recomp_trace.c do pcrecomp é Windows-only) ---- */
/* ---- overrides manuais (recomp_lookup_manual) ----
 * Funcoes cujo comeco verdadeiro nao e fronteira do catalogo (scrt, WinMain)
 * sao liftadas por tools/lift_override.py na CI e registradas aqui. */
#define DD_MAX_OVERRIDES 16
static struct { uint32_t va; recomp_func_t fn; } g_overrides[DD_MAX_OVERRIDES];
static int g_override_n;

void dd_overrides_add(uint32_t va, recomp_func_t fn)
{
    if (g_override_n < DD_MAX_OVERRIDES) {
        g_overrides[g_override_n].va = va;
        g_overrides[g_override_n].fn = fn;
        g_override_n++;
        fprintf(stderr, "[DesertDroid] override sub_%08X registrado\n", va);
    }
}

recomp_func_t recomp_lookup_manual(uint32_t va)
{
    for (int i = 0; i < g_override_n; i++)
        if (g_overrides[i].va == va) return g_overrides[i].fn;
    return NULL;
}

/* ---- trace de entry de funcoes (-DRECOMP_TRACE no codigo gerado) ----
 * Ring das ultimas 512 funcoes + dump no ExitProcess/crash; entradas em
 * codigo de jogo (VA < 0xE00000) imprimem na hora (ate o limite). */
#define DD_TRACE_RING 512
static uint32_t g_trace_ring[DD_TRACE_RING];
static uint32_t g_trace_idx, g_trace_total, g_trace_printed;

void dd_trace_dump(void)
{
    fprintf(stderr, "[trace] ultimas %u entradas (total %u):\n",
             g_trace_total < DD_TRACE_RING ? g_trace_total : (uint32_t)DD_TRACE_RING,
             g_trace_total);
    uint32_t start = g_trace_total < DD_TRACE_RING ? 0 : g_trace_idx;
    uint32_t count = g_trace_total < DD_TRACE_RING ? g_trace_total : DD_TRACE_RING;
    for (uint32_t k = 0; k < count; k++) {
        uint32_t va = g_trace_ring[(start + k) % DD_TRACE_RING];
        fprintf(stderr, "[trace] #%u sub_%08X\n", g_trace_total - count + k, va);
    }
}

void recomp_trace_enter(uint32_t va)
{
    g_trace_ring[g_trace_idx % DD_TRACE_RING] = va;
    g_trace_idx++;
    g_trace_total++;
    /* heartbeat: a cada 1000 funcoes guest, reportar onde estamos */
    if ((g_trace_total % 1000) == 0) {
        fprintf(stderr, "[hb %u] sub_%08X\n", g_trace_total, va);
        fflush(stderr);
    }
    if (va == 0x0086A850u)      /* WinMain REAL (mapeamento correto) */
        fprintf(stderr, "*** [WINMAIN] sub_0086A850 ALCANCADO! ***\n");
    if (va == 0x00EDD9E7u)      fprintf(stderr, "*** [scrt] conversao da linha de comando (00EDD9E7)\n");
    if (va == 0x00EDD92Cu)      fprintf(stderr, "*** [scrt] parse argv (00EDD92C)\n");
    if (va == 0x00EDD6B4u)      fprintf(stderr, "*** [scrt] parse cmdline (00EDD6B4)\n");
    if (va == 0x00ECCD14u)      fprintf(stderr, "*** [scrt] _initterm+checks (00ECCD14)\n");
    if (va == 0x00EDD655u)      fprintf(stderr, "*** [scrt] pre-WinMain (00EDD655)\n");
    if (va == 0x00ECCC55u) {
        extern uint32_t g_esp, g_eax, g_esi;
        extern uint32_t dd_mem32(uint32_t va);
        fprintf(stderr, "*** [scrt] _amsg_exit! (00ECCC55) — code=%u (arg da pilha), eax=%#x esi=%#x\n",
                dd_mem32(g_esp + 4), g_eax, g_esi);
    }
    if (va == 0x00EDDB9Au)      fprintf(stderr, "*** [scrt] cookie init (00EDDB9A)\n");
    if (va == 0x00ECC35Du)      fprintf(stderr, "*** [scrt] __scrt_common_main_seh (00ECC35D)\n");
    if (va >= 0x00401000u && va < 0x00EC0000u && g_trace_printed < 2000) {
        g_trace_printed++;
        fprintf(stderr, "[entry] sub_%08X\n", va);
    }
}
void recomp_dump_trace(const char *why) { (void)why; dd_trace_dump(); }
void recomp_trace_flush(void) {}
void recomp_trace_help(void) {}
int  recomp_trace_arg(int argc, char **argv, int i)
{
    (void)argc; (void)argv;
    return i;
}

/* leitura direta de memoria guest (modelo flat) para diagnostico */
uint32_t dd_mem32(uint32_t va)
{
    return *(uint32_t *)(uintptr_t)va;
}
