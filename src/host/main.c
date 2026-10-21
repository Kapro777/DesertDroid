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
#include <unistd.h>
#include "host.h"

/* estado do guest (definidos no host_lite/recomp_types.h) */
extern uint32_t    g_eax, g_ecx, g_edx, g_ebx, g_ebp, g_esi, g_edi, g_esp;
extern uint32_t    g_cur_func;
extern const char *g_cur_import;

void dd_shims_register(void);
void dd_kernel32_register(void);
void dd_sync_register(void);
void dd_locale_register(void);

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

    recomp_host_init();
    dd_shims_register();   /* ordinais: wsock32/dsound/xinput */
    dd_kernel32_register(); /* lote 1: tempo, interlocked, TLS, ids  */
    dd_sync_register();    /* critical sections (init de Gamebryo/Havok) */
    dd_locale_register();  /* codepage/locale (init do CRT MSVC)         */

    fprintf(stderr, "[DesertDroid] mapeando imagem: %s\n", argv[1]);

    /* boot permissivo: imports nao implementados viram stubs que se
     * anunciam quando chamados — o loop de bring-up do M3. */
    if (recomp_host_boot_permissive(argv[1]) != 0) {
        fprintf(stderr, "[DesertDroid] falha ao mapear/ligar a imagem\n");
        return 1;
    }

    fprintf(stderr, "[DesertDroid] imagem mapeada, entry VA %#x\n",
            recomp_host_entry());
    fprintf(stderr, "[DesertDroid] chamando entry (CRT -> WinMain)...\n");

    uint32_t rc = recomp_host_run();
    fprintf(stderr, "[DesertDroid] entry retornou %u\n", rc);
    return 0;
}
