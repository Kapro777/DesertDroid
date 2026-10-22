/* DesertDroid — lote 1 de kernel32 (fase de init do CRT).
 *
 * Cada um destes apareceu no censo de imports faltantes e pertence ao
 * caminho de boot (relogios, atomicos, TLS dinamico, ids). Implementados
 * sobre POSIX + arena de guest. stdcall: o argc do RET() tem que casar.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "win32hle.h"
#include "guest_mem.h"

/* ---- tempo ---- */
static uint64_t host_unix_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* FILETIME = 100ns desde 1601-01-01 */
static uint64_t to_filetime(uint64_t unix_ns)
{
    return unix_ns / 100ull + 116444736000000000ull;
}

/* GetSystemTimeAsFileTime(LPFILETIME) */
static void k_GetSystemTimeAsFileTime(void)
{
    uint64_t ft = to_filetime(host_unix_ns());
    uint64_t *p = (uint64_t *)APTR(0);
    *p = ft;
    RETV(1);
}

/* QueryPerformanceCounter(LARGE_INTEGER*) */
static void k_QueryPerformanceCounter(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *(uint64_t *)APTR(0) =
        (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    RET(1, 1);
}

/* QueryPerformanceFrequency(LARGE_INTEGER*) — 1 tick = 1ns */
static void k_QueryPerformanceFrequency(void)
{
    *(uint64_t *)APTR(0) = 1000000000ull;
    RET(1, 1);
}

typedef struct { uint16_t y, mo, dow, d, h, m, s, ms; } SYSTEMTIME_G;

static void fill_systime(SYSTEMTIME_G *st)
{
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    st->y = (uint16_t)(tm.tm_year + 1900);
    st->mo = (uint16_t)(tm.tm_mon + 1);
    st->dow = (uint16_t)tm.tm_wday;
    st->d = (uint16_t)tm.tm_mday;
    st->h = (uint16_t)tm.tm_hour;
    st->m = (uint16_t)tm.tm_min;
    st->s = (uint16_t)tm.tm_sec;
    st->ms = 0;
}

/* GetSystemTime(LPSYSTEMTIME) */
static void k_GetSystemTime(void)
{
    fill_systime((SYSTEMTIME_G *)APTR(0));
    RETV(1);
}

/* GetLocalTime(LPSYSTEMTIME) */
static void k_GetLocalTime(void)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    SYSTEMTIME_G *st = (SYSTEMTIME_G *)APTR(0);
    st->y = (uint16_t)(tm.tm_year + 1900);
    st->mo = (uint16_t)(tm.tm_mon + 1);
    st->dow = (uint16_t)tm.tm_wday;
    st->d = (uint16_t)tm.tm_mday;
    st->h = (uint16_t)tm.tm_hour;
    st->m = (uint16_t)tm.tm_min;
    st->s = (uint16_t)tm.tm_sec;
    st->ms = 0;
    RETV(1);
}

/* ---- ids ---- */
static void k_GetCurrentProcessId(void) { RET((uint32_t)getpid(), 0); }
static void k_GetCurrentThreadId(void)  { RET((uint32_t)syscall(SYS_gettid), 0); }
static void k_GetCurrentThread(void)     { RET(0xFFFFFFFEu, 0); }  /* pseudo-handle */

/* ---- Interlocked (atomicos nativos sobre memoria do guest) ---- */
static void k_InterlockedIncrement(void)
{
    int32_t *p = (int32_t *)APTR(0);
    RET((uint32_t)__sync_add_and_fetch(p, 1), 1);
}
static void k_InterlockedDecrement(void)
{
    int32_t *p = (int32_t *)APTR(0);
    RET((uint32_t)__sync_sub_and_fetch(p, 1), 1);
}
static void k_InterlockedExchange(void)
{
    int32_t *p = (int32_t *)APTR(0);
    uint32_t v = A32(1);
    RET((uint32_t)__atomic_exchange_n(p, (int32_t)v, __ATOMIC_SEQ_CST), 2);
}
static void k_InterlockedExchangeAdd(void)
{
    int32_t *p = (int32_t *)APTR(0);
    uint32_t v = A32(1);
    RET((uint32_t)__sync_fetch_and_add(p, (int32_t)v), 2);
}
static void k_InterlockedCompareExchange(void)
{
    int32_t *dst = (int32_t *)APTR(0);
    uint32_t exch = A32(1), cmp = A32(2);
    uint32_t got = (uint32_t)__sync_val_compare_and_swap(dst, (int32_t)cmp, (int32_t)exch);
    RET(got, 3);
}

/* ---- TLS dinamico: 64 slots por thread (host) ---- */
#define DD_TLS_SLOTS 64
static volatile uint32_t g_tls_used;
static __thread uint32_t g_tls_vals[DD_TLS_SLOTS];

static void k_TlsAlloc(void)
{
    for (int i = 0; i < DD_TLS_SLOTS; i++) {
        uint32_t bit = 1u << i;
        if (!(g_tls_used & bit) && !(__sync_fetch_and_or(&g_tls_used, bit) & bit)) {
            g_tls_vals[i] = 0;
            RET((uint32_t)i, 0);
        }
    }
    RET(0xFFFFFFFFu, 0);   /* TLS_OUT_OF_INDEXES */
}
static void k_TlsFree(void)  { __sync_fetch_and_and(&g_tls_used, ~(1u << A32(0))); RET(1, 1); }
static void k_TlsGetValue(void) { RET(g_tls_vals[A32(0) & (DD_TLS_SLOTS - 1)], 1); }
static void k_TlsSetValue(void) { g_tls_vals[A32(0) & (DD_TLS_SLOTS - 1)] = A32(1); RET(1, 2); }

/* ---- heap/handle ---- */
static void k_HeapCreate(void) { RET(0x00D0D000u, 3); }   /* handle fake       */
static void k_HeapSetInformation(void) { RET(1, 4); }     /* HeapSetInformation? nao importado; reserva */
static void k_TerminateProcess(void) { _exit((int)A32(1)); }

/* ---- modulo/console ---- */
static void k_GetModuleHandleW(void) { RET(0x00400000u, 1); }
static void k_SetEnvironmentVariableA(void) { RET(1, 2); }

/* ---- memoria / sistema (init do engine BGS/Gamebryo) ---- */
typedef struct {
    uint32_t dwLength, dwMemoryLoad;
    uint64_t ullTotalPhys, ullAvailPhys;
    uint64_t ullTotalPageFile, ullAvailPageFile;
    uint64_t ullTotalVirtual, ullAvailVirtual;
    uint64_t ullAvailExtendedVirtual;
} MEMORYSTATUSEX_G;

static void k_GlobalMemoryStatusEx(void)
{
    MEMORYSTATUSEX_G *m = (MEMORYSTATUSEX_G *)APTR(0);
    if (!m || m->dwLength < sizeof *m) RET(0, 1);
    m->dwMemoryLoad  = 50;
    m->ullTotalPhys = 2ull << 30;      /* 2 GB enderecaveis (exe sem LAA) */
    m->ullAvailPhys  = 1536ull << 20;
    m->ullTotalPageFile  = m->ullTotalPhys;
    m->ullAvailPageFile  = m->ullAvailPhys;
    m->ullTotalVirtual   = 0x7FFEFFFFull;
    m->ullAvailVirtual   = 1536ull << 20;
    m->ullAvailExtendedVirtual = 0;
    RET(1, 1);
}

typedef struct {
    uint16_t wProcessorArchitecture, wReserved;
    uint32_t dwPageSize, lpMinimumApplicationAddress;
    uint32_t lpMaximumApplicationAddress, dwActiveProcessorMask;
    uint32_t dwNumberOfProcessors, dwProcessorType;
    uint16_t wProcessorLevel, wProcessorRevision;
} SYSTEM_INFO_G;

static void k_GetSystemInfo(void)
{
    SYSTEM_INFO_G *s = (SYSTEM_INFO_G *)APTR(0);
    s->wProcessorArchitecture = 0;               /* PROCESSOR_ARCHITECTURE_INTEL */
    s->dwPageSize               = 4096;
    s->lpMinimumApplicationAddress = 0x10000;
    s->lpMaximumApplicationAddress = 0x7FFEFFFF;
    s->dwActiveProcessorMask    = 0xFF;
    s->dwNumberOfProcessors     = 4;
    s->dwProcessorType          = 586;           /* PROCESSOR_INTEL_PENTIUM */
    s->wProcessorLevel          = 6;
    s->wProcessorRevision       = 0;
    RETV(1);
}

/* timeGetTime: milissegundos monotonicos (o engine chama isso o tempo todo) */
static void k_timeGetTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    RET((uint32_t)((uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull), 0);
}

/* SHGetFolderPathA(hwnd, nFolder, hToken, dwFlags, pszPath) -> S_OK */
static void k_SHGetFolderPathA(void)
{
    char *path = (char *)APTR(4);
    if (path) strcpy(path, "C:\\Users\\Player\\Documents");
    RET(0, 5);                                   /* S_OK */
}

/* GetUserNameA(lpBuffer, lpnSize) */
static void k_GetUserNameA(void)
{
    char *buf = (char *)APTR(0);
    uint32_t *nsize = (uint32_t *)APTR(1);
    if (!buf || !nsize) RET(0, 2);
    strcpy(buf, "DesertDroid");
    *nsize = 12;                                 /* com NUL */
    RET(1, 2);
}

/* EncodePointer/DecodePointer: identidade (sem ASLR no modelo flat do
 * guest, encodar e' devolver o mesmo ponteiro). O CRT VS2008 procura os
 * dois via GetProcAddress e CHAMA o que achar — NULL = crash depois. */
static void k_EncodePointer(void) { RETP(A32(0), 1); }
static void k_DecodePointer(void) { RETP(A32(0), 1); }

/* IsProcessorFeaturePresent(n) — via GetProcAddress; coerente com o CPUID */
static void k_IsProcessorFeaturePresent(void)
{
    uint32_t f = A32(0);
    /* 0 FP errata, 2 CMPXCHG8B, 3 MMX, 8 RDTSC, 9 XMMI(SSE), 10 XMMI2(SSE2) */
    static const uint32_t supported = (1u << 0) | (1u << 2) | (1u << 3) |
                                      (1u << 8) | (1u << 9) | (1u << 10);
    RET(f < 32 && (supported & (1u << f)) ? 1u : 0u, 1);
}

static const win32hle_shim dd_shims_kernel32[] = {
    { "GetSystemTimeAsFileTime", k_GetSystemTimeAsFileTime },
    { "QueryPerformanceCounter", k_QueryPerformanceCounter },
    { "QueryPerformanceFrequency", k_QueryPerformanceFrequency },
    { "GetSystemTime",           k_GetSystemTime },
    { "GetLocalTime",            k_GetLocalTime },
    { "GetCurrentProcessId",     k_GetCurrentProcessId },
    { "GetCurrentThreadId",      k_GetCurrentThreadId },
    { "GetCurrentThread",        k_GetCurrentThread },
    { "InterlockedIncrement",    k_InterlockedIncrement },
    { "InterlockedDecrement",    k_InterlockedDecrement },
    { "InterlockedExchange",     k_InterlockedExchange },
    { "InterlockedExchangeAdd",  k_InterlockedExchangeAdd },
    { "InterlockedCompareExchange", k_InterlockedCompareExchange },
    { "TlsAlloc",                k_TlsAlloc },
    { "TlsFree",                 k_TlsFree },
    { "TlsGetValue",             k_TlsGetValue },
    { "TlsSetValue",             k_TlsSetValue },
    { "HeapCreate",              k_HeapCreate },
    { "GlobalMemoryStatusEx",    k_GlobalMemoryStatusEx },
    { "GetSystemInfo",           k_GetSystemInfo },
    { "timeGetTime",             k_timeGetTime },
    { "SHGetFolderPathA",        k_SHGetFolderPathA },
    { "GetUserNameA",            k_GetUserNameA },
    { "IsProcessorFeaturePresent", k_IsProcessorFeaturePresent },
    { "EncodePointer",           k_EncodePointer },
    { "DecodePointer",           k_DecodePointer },
    /* FLS = TLS no nosso modelo (o CRT VS2008 procura via GetProcAddress) */
    { "FlsAlloc",                k_TlsAlloc },
    { "FlsFree",                 k_TlsFree },
    { "FlsGetValue",             k_TlsGetValue },
    { "FlsSetValue",             k_TlsSetValue },
    { "TerminateProcess",        k_TerminateProcess },
    { "GetModuleHandleW",        k_GetModuleHandleW },
    { "SetEnvironmentVariableA",  k_SetEnvironmentVariableA },
    { NULL, NULL }
};

void dd_kernel32_register(void)
{
    win32hle_register(dd_shims_kernel32);
}
