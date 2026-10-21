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
void recomp_trace_enter(uint32_t va) { (void)va; }
void recomp_dump_trace(const char *why) { (void)why; }
void recomp_trace_flush(void) {}
void recomp_trace_help(void) {}
int  recomp_trace_arg(int argc, char **argv, int i)
{
    (void)argc; (void)argv;
    return i;
}
