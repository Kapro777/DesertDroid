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
#include <pthread.h>
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
void dd_crt_overrides_register(void);
void dd_threads_register(void);
void dd_tracers_register(void);
void dd_overrides_add(uint32_t va, recomp_func_t fn);
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
    {
        /* leitura SEGURA: ebp pode ser lixo (1) — validar antes de ler */
        extern uint32_t dd_mem32(uint32_t va);
        #define SAFE32(va) (((va) >= 0x1000u && (va) < 0xF0000000u) ? dd_mem32(va) : 0xDEADBEEFu)
        fprintf(stderr, "  pilha: [ebp+8]=%#x [ebp+C]=%#x [ebp+10]=%#x [ebp+14]=%#x [ebp+18]=%#x\n",
                SAFE32(g_ebp + 8), SAFE32(g_ebp + 0xC), SAFE32(g_ebp + 0x10),
                SAFE32(g_ebp + 0x14), SAFE32(g_ebp + 0x18));
        fprintf(stderr, "  frame local: [ebp-14]=%#x [ebp-10]=%#x [ebp-C]=%#x [ebp-8]=%#x [ebp-4]=%#x\n",
                SAFE32(g_ebp - 0x14), SAFE32(g_ebp - 0x10), SAFE32(g_ebp - 0xC),
                SAFE32(g_ebp - 0x8), SAFE32(g_ebp - 0x4));
        fprintf(stderr, "  MM singleton [0x11F6080]=%#x -> heap_obj=%#x -> [heap_obj]=%#x\n",
                SAFE32(0x11F6080u),
                (SAFE32(0x11F6080u) != 0xDEADBEEFu) ? SAFE32(SAFE32(0x11F6080u)) : 0xDEADBEEFu,
                (SAFE32(0x11F6080u) != 0xDEADBEEFu) ? SAFE32(SAFE32(SAFE32(0x11F6080u))) : 0xDEADBEEFu);
        {
            uint32_t s = dd_mem32(0x11F6080u);
            if (s != 0xDEADBEEFu && s >= 0x1000u && s < 0xF0000000u) {
                uint32_t ho = dd_mem32(s);          /* heap_obj */
                if (ho >= 0x1000u && ho < 0xF0000000u) {
                    uint32_t vt = dd_mem32(ho);     /* vtable REAL (0x10A24FC) */
                    if (vt >= 0x1000u && vt < 0xF0000000u) {
                        fprintf(stderr, "  vtable real %#x: [+0]=%#x [+4]=%#x [+8]=%#x [+0x10]=%#x (alloc cmd 0xD) [+0x14]=%#x (free) [+0x18]=%#x (realloc)\n",
                                vt, dd_mem32(vt), dd_mem32(vt+4), dd_mem32(vt+8),
                                dd_mem32(vt+0x10), dd_mem32(vt+0x14), dd_mem32(vt+0x18));
                        fprintf(stderr, "  globals MM: [0x11F6118]=%#x [0x11F622C]=%#x [0x11F610D]=%#x [0x11F6110]=%#x [0x11F6238]=%#x\n",
                                SAFE32(0x11F6118u), SAFE32(0x11F622Cu), SAFE32(0x11F610Du), SAFE32(0x11F6110u), SAFE32(0x11F6238u));
                        /* estado do slab 0 do pool (0xAA35A0 le esses campos) */
                        {
                            uint32_t pool = dd_mem32(0x11F622Cu);
                            if (pool >= 0x1000u && pool < 0xF0000000u)
                                fprintf(stderr, "  pool slab[0]: [0]=%#x [4]=%#x [8]=%#x [C]=%#x [10]=%#x [14]=%#x [1C]=%#x\n",
                                        dd_mem32(pool), dd_mem32(pool+4), dd_mem32(pool+8),
                                        dd_mem32(pool+0xC), dd_mem32(pool+0x10),
                                        dd_mem32(pool+0x14), dd_mem32(pool+0x1C));
                        }
                    } else {
                        fprintf(stderr, "  (vtable INVALIDA: [heap_obj]=%#x)\n", vt);
                    }
                } else {
                    fprintf(stderr, "  (heap_obj INVALIDO: %x)\n", ho);
                }
            }
        }
    }
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

/* ---- monitor: thread separada que reporta onde o guest esta ---- */
static volatile uint32_t g_mon_run = 1;
static void *dd_monitor_thread(void *arg)
{
    (void)arg;
    extern uint32_t g_cur_func;
    extern const char *g_cur_import;
    fprintf(stderr, "[mon] thread iniciada! g_mon_run=%u\n", g_mon_run);
    fflush(stderr);
    for (int i = 0; i < 3000; i++) {
        fprintf(stderr, "[mon %d PRE] i=%d\n", i, i);  /* ANTES do usleep */
        fflush(stderr);
        usleep(100000);
        fprintf(stderr, "[mon %d] sub_%08X import=%s\n",
                i, g_cur_func, g_cur_import ? g_cur_import : "-");
        fflush(stderr);
    }
    fprintf(stderr, "[mon] thread finalizando\n");
    return NULL;
}

static void dd_install_sampler(void)
{
    pthread_t t;
    int rc = pthread_create(&t, NULL, dd_monitor_thread, NULL);
    fprintf(stderr, "[mon] pthread_create = %d\n", rc);
    fflush(stderr);
    if (rc == 0) pthread_detach(t);
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
    dd_crt_overrides_register(); /* _initterm sem o callback assassino     */
    dd_threads_register();      /* CreateThread/ExitThread (MM workers)   */
    dd_tracers_register();      /* descobrir quem chama os inits do MM   */

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

    /* buffer estatico do jogo ([0x11F42C0]): o jogo le pool+0x2080 como
     * string (A5B690). Sem isso o format id=3 crasha 4 niveis abaixo.
     * NAO e' inicializado por nenhum codigo do exe — vem do estado do
     * loader que nos nao replicamos (72KB zerados bastam p/ strlen=0). */
    {
        extern uint32_t guest_alloc(size_t size);
        *(uint32_t *)(uintptr_t)0x11F42C0u = guest_alloc(0x10000);
        fprintf(stderr, "[DesertDroid] buffer estatico 0x11F42C0 pre-alocado\n");
    }

    /* buffer estatico do jogo ([0x11F42C0]): o ctor que o aloca
     * (sub_00A5AA20) nao roda no nosso caminho de boot; pre-alocamos
     * zerado (a imagem ja esta mapeada aqui) — o consumidor le vazio. */
    {
        extern uint32_t guest_alloc(size_t size);
        *(uint32_t *)(uintptr_t)0x11F42C0u = guest_alloc(0x10000);
        fprintf(stderr, "[DesertDroid] buffer estatico 0x11F42C0 pre-alocado\n");
    }

    /* Boot PELO ENTRY REAL do PE (sub_00ECC4DB: call cookie; jmp scrt —
     * catalogado e limpo; as analises "adulteradas" eram offset manual
     * errado de 0xC00). O trace grita cada etapa do scrt. */
    fprintf(stderr, "[DesertDroid] chamando entry (CRT -> WinMain)...\n");

    fprintf(stderr, "[DesertDroid] chamando entry (CRT -> WinMain)...\n");

    uint32_t rc = recomp_host_run();
    fprintf(stderr, "[DesertDroid] entry retornou %u\n", rc);
    return 0;
}
