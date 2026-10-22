/* DesertDroid — host principal (M2/M3)
 *
 * Inicializa o runtime portavel (win32hle), mapeia o FalloutNV.exe
 * DO USUARIO (BYOG — a copia dele, nunca distribuida), resolve os
 * imports (nomes E ordinais, via shims DesertDroid), chama o entry
 * point liftado — e reporta qualquer crash com o estado do guest.
 */
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "host.h"
#include "recomp_types.h"

/* estado do guest (definidos no host_lite/recomp_types.h) */
extern uint32_t    g_eax, g_ecx, g_edx, g_ebx, g_ebp, g_esi, g_edi, g_esp;
extern uint32_t    g_cur_func;
extern const char *g_cur_import;

void dd_shims_register(void);
void dd_kernel32_register(void);
void dd_sync_register(void);
void dd_locale_register(void);
void dd_fs_register(void);
void dd_d3d9_register(void);
void dd_dynload_register(void);
void dd_fs_set_root(const char *host_dir);
extern int win32hle_trace;   /* rastreamento de imports do proprio win32hle */

/* ---- crash report: todo sinal fatal nomeia a funcao liftada ---- */
static void dd_crash(int sig, siginfo_t *si, void *ctx)
{
    (void)ctx;
    fprintf(stderr, "\n[DesertDroid][CRASH] sinal %d em %p\n",
            sig, si ? si->si_addr : NULL);
    fprintf(stderr, "  funcao guest: sub_%08X", g_cur_func);
    if (g_cur_import)
        fprintf(stderr, "  (import: %s)", g_cur_import);
    fprintf(stderr, "\n");
    fprintf(stderr, "  eax=%08X ecx=%08X edx=%08X ebx=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx);
    fprintf(stderr, "  esp=%08X ebp=%08X esi=%08X edi=%08X\n",
            g_esp, g_ebp, g_esi, g_edi);
    fprintf(stderr, "  (mapear sub_XXXXXX em work/names.json ou no catalogo)\n");
    {
        extern void dd_trace_dump(void);
        dd_trace_dump();
    }
    _exit(128 + sig);
}

static void dd_install_crash_report(void)
{
    struct sigaction sa;
    sa.sa_sigaction = dd_crash;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

/* ---- sampler: a cada 1s, onde o guest esta? (profiler caseiro do boot) ---- */
static volatile uint64_t g_sample_n;
static void dd_sample(int sig)
{
    (void)sig;
    uint64_t n = ++g_sample_n;
    fprintf(stderr, "[sample %llu] t=%llus funcao sub_%08X import=%s\n",
            (unsigned long long)n, (unsigned long long)n,
            g_cur_func, g_cur_import ? g_cur_import : "-");
}

static void dd_install_sampler(void)
{
    struct sigaction sa;
    sa.sa_handler = dd_sample;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    struct itimerval it;
    it.it_value.tv_sec = 0;    it.it_value.tv_usec = 100000;    /* 100 ms */
    it.it_interval.tv_sec = 0; it.it_interval.tv_usec = 100000;
    setitimer(ITIMER_REAL, &it, NULL);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
            "DesertDroid (M3) — Fallout: New Vegas nativo (bring-your-own-game)\n"
            "uso: %s <caminho/do/FalloutNV.exe>\n"
            "BYOG: use a SUA copia do jogo.\n", argv[0]);
        return 2;
    }

    dd_install_crash_report();
    dd_install_sampler();
    win32hle_trace = 1;    /* [hle] Nome — cada import chamado no boot */

    recomp_host_init();
    dd_shims_register();   /* ordinais: wsock32/dsound/xinput */
    dd_kernel32_register(); /* lote 1: tempo, interlocked, TLS, ids  */
    dd_sync_register();    /* critical sections (init de Gamebryo/Havok) */
    dd_locale_register();  /* codepage/locale (init do CRT MSVC)         */
    dd_fs_register();      /* filesystem: FindFirstFile, pastas, ExitProcess diag */
    dd_dynload_register(); /* GetProcAddress de verdade                    */
    dd_d3d9_register();     /* Direct3DCreate9 + COM HLE auto-anunciante    */

    /* raiz de arquivos = pasta do exe do usuario (BYOG) */
    {
        static char dirbuf[1024];
        snprintf(dirbuf, sizeof dirbuf, "%s", argv[1]);
        char *slash = strrchr(dirbuf, '/');
        if (slash) *slash = 0;
        else strcpy(dirbuf, ".");
        dd_fs_set_root(dirbuf);
    }

    fprintf(stderr, "[DesertDroid] mapeando imagem: %s\n", argv[1]);

    /* boot permissivo: imports nao implementados viram stubs que se
     * anunciam quando chamados — o loop de bring-up do M3. */
    if (recomp_host_boot_permissive(argv[1]) != 0) {
        fprintf(stderr, "[DesertDroid] falha ao mapear/ligar a imagem\n");
        return 1;
    }

    fprintf(stderr, "[DesertDroid] imagem mapeada, entry VA %#x\n",
            recomp_host_entry());

    /* O entry do PE deste exe e' adulterado (SteamRIP: fragmento de 10 bytes).
     * Boot no scrt REAL (0xECB75D) — o comeco verdadeiro nao e fronteira do
     * catalogo (o descent fundiu com funcoes matematicas do CRT), entao a CI
     * o lifta via tools/lift_override.py e registra na tabela de overrides. */
    {
        extern uint32_t hle_call_guest(uint32_t va, int nargs, const uint32_t *args);
        extern recomp_func_t recomp_lookup(uint32_t va);   /* recomp_types.h */
        /* gerados na CI (weak: linka mesmo sem eles presentes) */
        __attribute__((weak)) void dd_override_register_00ECB75D(void);
        __attribute__((weak)) void dd_override_register_00869C50(void);
        if (dd_override_register_00ECB75D) dd_override_register_00ECB75D();
        if (dd_override_register_00869C50) dd_override_register_00869C50();
        if (recomp_lookup(0x00ECB75Du)) {
            fprintf(stderr, "[DesertDroid] boot no scrt real (override sub_00ECB75D)...\n");
            uint32_t rc = hle_call_guest(0x00ECB75Du, 0, NULL);
            fprintf(stderr, "[DesertDroid] fluxo principal retornou %u\n", rc);
            return (int)rc;
        }
        fprintf(stderr, "[DesertDroid] override do scrt ausente — boot pelo entry do PE...\n");
    }
    fprintf(stderr, "[DesertDroid] chamando entry (CRT -> WinMain)...\n");

    fprintf(stderr, "[DesertDroid] chamando entry (CRT -> WinMain)...\n");

    uint32_t rc = recomp_host_run();
    fprintf(stderr, "[DesertDroid] entry retornou %u\n", rc);
    return 0;
}
