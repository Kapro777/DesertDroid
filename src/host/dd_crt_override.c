/* DesertDroid — overrides cirurgicos do CRT VS2008 (a camada mais rasa).
 *
 * 00ECCD14 (wrapper _initterm): o tail original chama um callback de
 *   terminacao ([0x1271A94]) registrado durante os construtores, que
 *   dispara exit(0) PRE-WinMain (run #30). Override: roda o _initterm
 *   REAL via hle_call_guest e pula o callback.
 * 00ED0F17 (getter de per-thread data): fast path original depende de
 *   ponteiro dinamico ([0xFDF044]) que nao satisfazemos; a fallback
 *   aborta(8) "not enough space for thread data" — mesmo com o ptd JA
 *   EXISTINDO (run #33: slot 1 = 0x50401030 estavel). Override: le o
 *   indice da global do CRT ([0x11C1CC0]) e devolve o valor do slot.
 *
 * Somente codigo DesertDroid aqui; _initterm e codigo LIFTADO, chamado
 * pelo dispatch normal (BYOG — nunca e distribuido).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include "win32hle.h"

extern uint32_t g_eax;
extern void dd_overrides_add(uint32_t va, recomp_func_t fn);
uint32_t dd_tls_slot_value(uint32_t idx);

/* _initterm real = sub_00ECCCF0 (catalogado); args do original:
 *   push 0x10113E8 (__xi_z); push 0x10113C4 (__xi_a); call _initterm */
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
    /* O check do scrt (0xecc445) compara eax com esi. Retorna g_esi:
     * igual-com-igual passa por construcao, qualquer que seja o valor. */
    extern uint32_t g_esi;
    g_eax = g_esi;
    fprintf(stderr, "[DesertDroid] override retorna eax=esi=%#x\n", g_esi);
}

static void dd_ovr_00ED0F17(void)
{
    /* O ptd e' o UNICO ponteiro na faixa da arena (0x50000000-0x68000000)
     * nos slots TLS (run #33: slot 1 = 0x50401030 estavel). Scan semantico:
     * nao dependo de qual global do CRT guarda o indice. */
    extern uint32_t dd_tls_slots_snapshot(uint32_t *out, uint32_t n);
    uint32_t vals[64];
    uint32_t n = dd_tls_slots_snapshot(vals, 64);
    uint32_t ptd = 0, slot = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        if (vals[i] >= 0x50000000u && vals[i] < 0x68000000u) {
            ptd = vals[i];
            slot = i;
            break;
        }
    }
    fprintf(stderr, "[DesertDroid] override 00ED0F17: ptd slot %u -> %#x\n",
            slot, ptd);
    g_eax = ptd;                                 /* 0 = quem chama decide  */
}

/* sub_00A5B5A0: format/get-string por ID no TLS do jogo. O MM chama com
 * (buffer_dest, id). Sem o string-manager inicializado (init que nao roda
 * no nosso caminho), o original crasha 4 niveis abaixo. Resposta util:
 * string vazia no buffer (caller usa o conteudo) + buffer como retorno. */
static void dd_ovr_00A5B5A0(void)
{
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_esp;
    uint32_t buf = dd_mem32(g_esp + 4);
    uint32_t id  = dd_mem32(g_esp + 8);
    if (buf)
        *(uint32_t *)(uintptr_t)buf = 0;         /* "" no buffer destino */
    fprintf(stderr, "[DesertDroid] format-string id=%u -> \"\" (buffer %#x)\n",
            id, buf);
    g_eax = buf;                                 /* retorno = buffer */
}

void dd_crt_overrides_register(void)
{
    dd_overrides_add(0x00ECCD14u, dd_ovr_00ECCD14);
    dd_overrides_add(0x00A5B5A0u, dd_ovr_00A5B5A0);
    fprintf(stderr, "[DesertDroid] overrides CRT registrados "
            "(00ECCD14 _initterm, 00A5B5A0 format-string)\n");
}
