/* DesertDroid — critical sections (fase de init de Gamebryo/Havok).
 *
 * O guest guarda um RTL_CRITICAL_SECTION de 24 bytes; usamos um pool de
 * mutexes recursivos POSIX e guardamos o indice no campo [0] do struct
 * (SpinCount/DebugInfo seguem nos campos originais sem uso pro host).
 * Como os shims rodam com o machine lock do guest ja tomado, usar um
 * segundo mutex aqui e seguro (locks diferentes).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include "win32hle.h"

#define CS_POOL 512
static pthread_mutex_t g_cs_pool[CS_POOL];
static volatile uint32_t g_cs_used;

static pthread_mutex_t *cs_from(uint32_t addr)
{
    uint32_t *cs = (uint32_t *)(uintptr_t)addr;
    uint32_t idx = cs[0];
    if (idx == 0 || idx > CS_POOL) return NULL;  /* slot 0 nunca e usado */
    return &g_cs_pool[idx - 1];
}

static void cs_init_one(uint32_t addr)
{
    uint32_t *cs = (uint32_t *)(uintptr_t)addr;
    for (uint32_t i = 0; i < CS_POOL; i++) {
        uint32_t bit = 1u << (i % 32), word = i / 32;
        /* bitmap simples, 16 palavras */
        static volatile uint32_t used[16];
        if (!(used[word] & bit) &&
            !(__sync_fetch_and_or(&used[word], bit) & bit)) {
            pthread_mutex_t *m = &g_cs_pool[i];
            pthread_mutexattr_t a;
            pthread_mutexattr_init(&a);
            pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(m, &a);
            pthread_mutexattr_destroy(&a);
            memset((void *)(uintptr_t)addr, 0, 24);
            cs[0] = i + 1;         /* indice (0 = invalido)   */
            cs[2] = 0xFFFFFFFFu;   /* RecursionCount/Owner pinificado */
            return;
        }
    }
    fprintf(stderr, "[DesertDroid] pool de critical sections esgotado\n");
}

/* InitializeCriticalSection(LPCRITICAL_SECTION) */
static void k_InitializeCriticalSection(void)
{
    cs_init_one(A32(0));
    RETV(1);
}

/* InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION, DWORD) */
static void k_InitializeCriticalSectionAndSpinCount(void)
{
    cs_init_one(A32(0));
    RET(1, 2);
}

/* EnterCriticalSection(LPCRITICAL_SECTION)
 * Com o yield hook ativo, a maquina pode ser entregue DENTRO de uma
 * secao critica (backedge de um loop interno). Se a outra thread tenta
 * a MESMA CS enquanto detem a maquina, o pthread_mutex_lock bloquearia
 * com a maquina detida = deadlock. Fast path: trylock. Se disputado,
 * solta a maquina, bloqueia e reivindica (padrao native32). */
static void k_EnterCriticalSection(void)
{
    pthread_mutex_t *m = cs_from(A32(0));
    if (m) {
        extern void mach_enter(void);
        extern void mach_leave(void);
        if (pthread_mutex_trylock(m) != 0) {
            mach_leave();
            pthread_mutex_lock(m);
            mach_enter();
        }
    }
    RETV(1);
}

/* LeaveCriticalSection(LPCRITICAL_SECTION) */
static void k_LeaveCriticalSection(void)
{
    pthread_mutex_t *m = cs_from(A32(0));
    if (m) pthread_mutex_unlock(m);
    RETV(1);
}

/* TryEnterCriticalSection(LPCRITICAL_SECTION) -> BOOL */
static void k_TryEnterCriticalSection(void)
{
    pthread_mutex_t *m = cs_from(A32(0));
    int ok = m ? (pthread_mutex_trylock(m) == 0) : 0;
    RET((uint32_t)ok, 1);
}

/* DeleteCriticalSection(LPCRITICAL_SECTION) */
static void k_DeleteCriticalSection(void)
{
    pthread_mutex_t *m = cs_from(A32(0));
    if (m) pthread_mutex_unlock(m);   /* devolve ao estado livre */
    RETV(1);
}

static const win32hle_shim dd_shims_sync[] = {
    { "InitializeCriticalSection",              k_InitializeCriticalSection },
    { "InitializeCriticalSectionAndSpinCount",  k_InitializeCriticalSectionAndSpinCount },
    { "EnterCriticalSection",                    k_EnterCriticalSection },
    { "LeaveCriticalSection",                   k_LeaveCriticalSection },
    { "TryEnterCriticalSection",                k_TryEnterCriticalSection },
    { "DeleteCriticalSection",                  k_DeleteCriticalSection },
    { NULL, NULL }
};

void dd_sync_register(void)
{
    win32hle_register(dd_shims_sync);
}
