/* DesertDroid — threads guest (CreateThread/ExitThread).
 *
 * Cada thread guest e' uma pthread host; o trampoline chama o start
 * routine do guest via hle_call_guest (que cria TIB/stack per-thread
 * e, via hook dd_on_tib_ready, monta o TLS estatico da thread).
 * O machine lock serializa o guest entre threads (design cooperativo
 * do pcrecomp — os yields nos backward-branches dao progresso a todos).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include "win32hle.h"

extern uint32_t hle_call_guest(uint32_t va, int nargs, const uint32_t *args);
extern void mach_leave(void);

typedef struct { uint32_t start, param; } dd_thread_arg;

static void *dd_thread_trampoline(void *p)
{
    dd_thread_arg a = *(dd_thread_arg *)p;
    free(p);
    uint32_t rc = hle_call_guest(a.start, 1, &a.param);
    fprintf(stderr, "[thread] start sub_%08X retornou %u — encerrando\n",
            a.start, rc);
    return NULL;
}

static volatile uint32_t g_thread_seq = 0x100;

/* CreateThread(attrs, stack_size, start, param, flags, tid_out) */
static void k_CreateThread(void)
{
    uint32_t start = A32(2), param = A32(3), tid_out = A32(5);
    dd_thread_arg *a = malloc(sizeof *a);
    if (!a) RET(0, 6);
    a->start = start;
    a->param = param;
    pthread_t t;
    if (pthread_create(&t, NULL, dd_thread_trampoline, a) != 0) {
        free(a);
        RET(0, 6);
    }
    pthread_detach(t);
    uint32_t tid = __sync_fetch_and_add(&g_thread_seq, 1);
    if (tid_out)
        *(uint32_t *)(uintptr_t)tid_out = tid;
    fprintf(stderr, "[thread] criada sub_%08X (tid %u)\n", start, tid);
    RET(tid, 6);                    /* handle = tid (pseudo) */
}

static void k_ExitThread(void)
{
    fprintf(stderr, "[thread] ExitThread(%u)\n", A32(0));
    mach_leave();                   /* solta o machine lock */
    pthread_exit(NULL);             /* a pthread termina; lock liberado */
}

static const win32hle_shim dd_shims_threads[] = {
    { "CreateThread", k_CreateThread },
    { "ExitThread",   k_ExitThread },
    { NULL, NULL }
};

void dd_threads_register(void)
{
    win32hle_register(dd_shims_threads);
}
