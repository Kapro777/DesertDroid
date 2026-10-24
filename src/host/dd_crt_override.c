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
static void dd_ovr_00AA35A0(void);
static void dd_ovr_00A5B270(void);
uint32_t dd_tls_slot_value(uint32_t idx);

/* _initterm real = sub_00ECCCF0 (catalogado); args do original:
 *   push 0x10113E8 (__xi_z); push 0x10113C4 (__xi_a); call _initterm */
#define XI_A 0x10113C4u
#define XI_Z 0x10113E8u
#define INITERM_VA 0x00ECCCF0u

static void dd_ovr_00ECCD14(void)
{
    extern uint32_t dd_mem32(uint32_t va);
    fprintf(stderr, "[DesertDroid] override 00ECCD14: _initterm(xi)...\n");
    {
        const uint32_t args[2] = { XI_A, XI_Z };
        hle_call_guest(INITERM_VA, 2, args);   /* stdcall: push xi_z, xi_a */
    }
    fprintf(stderr, "[DesertDroid] _initterm completou.\n");
    /* Pool MM: os construtores (que acabaram de rodar) criam o pool via
     * vtable (sub_00AA3430) e o escrevem em [0x11F622C]. Mas o sub_00AA3760
     * so inicializa os SLOTS, nao o header que sub_00AA35A0 le.
     * Completar o header DEPOIS dos construtores, ANTES do boot. */
    {
        extern uint32_t dd_mem32(uint32_t va);
        extern uint32_t guest_alloc(size_t size);
        uint32_t global_pool = dd_mem32(0x11F622Cu);
        if (global_pool >= 0x50000000u && global_pool < 0x68000000u &&
            dd_mem32(global_pool + 8) == 0) {
            *(uint32_t *)(uintptr_t)(global_pool + 8)    = global_pool + 0x80;
            *(uint32_t *)(uintptr_t)(global_pool + 0x10) = 256;
            *(uint32_t *)(uintptr_t)(global_pool + 4)    = 8;
            fprintf(stderr, "[DesertDroid] pool header completado: pool=%#x "
                            "slots=%#x cap=256\n",
                    global_pool, global_pool + 0x80);
        }
        /* heap Havok: [0x11F6118] = container -> heap_obj (vtable real) */
        if (dd_mem32(0x11F6118u) == 0 ||
            dd_mem32(0x11F6118u) < 0x50000000u) {
            uint32_t heap_obj = guest_alloc(0x100);
            *(uint32_t *)(uintptr_t)heap_obj = 0x010A24FCu;
            uint32_t container = guest_alloc(0x10);
            *(uint32_t *)(uintptr_t)container = heap_obj;
            *(uint8_t *)(uintptr_t)(container + 4) = 1;
            *(uint32_t *)(uintptr_t)0x11F6118u = container;
            fprintf(stderr, "[DesertDroid] heap montado: container=%#x "
                            "heap=%#x vtable=0x10A24FC\n",
                    container, heap_obj);
        }
    }
    /* O check do scrt (0xecc445) compara eax com esi. Retorna g_esi. */
    extern uint32_t g_esi;
    g_eax = g_esi;
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
    /* Completar o header do pool MM natural: o boot ja criou o pool via
     * sub_00AA3430 (vtable virtual) mas o sub_00AA3760 so inicializa os
     * SLOTS, nao o header que sub_00AA35A0 le. Se [0x11F622C] e' um
     * pool valido sem header, completar os campos. */
    {
        extern uint32_t dd_mem32(uint32_t va);
        uint32_t pool = dd_mem32(0x11F622Cu);
        if (pool >= 0x50000000u && pool < 0x68000000u &&
            dd_mem32(pool + 8) == 0) {
            *(uint32_t *)(uintptr_t)(pool + 8)    = pool + 0x80;
            *(uint32_t *)(uintptr_t)(pool + 0x10) = 256;
            *(uint32_t *)(uintptr_t)(pool + 4)    = 8;
            fprintf(stderr, "[DesertDroid] pool header completado: "
                            "pool=%#x slots=%#x cap=256\n",
                    pool, pool + 0x80);
        }
    }
    fprintf(stderr, "[DesertDroid] format-string id=%u -> \"\" (buffer %#x)\n",
            id, buf);
    g_eax = buf;                                 /* retorno = buffer */
}

/* ---- rotinas de memoria do CRT substituidas por nativas do host ----
 * O memset liftado (00EC61C0) tem um tail-jmp (jmp 0xed2cd5) cujo frame
 * e o suspeito do desalinhamento (ebx=DEAD0000 = RETADDR vazado). As
 * operacoes sao triviais no host — mais rapidas e sem risco de lifter. */
static void dd_ovr_memset(void)
{
    extern uint32_t g_esp;
    extern uint32_t dd_mem32(uint32_t va);
    uint32_t dst = dd_mem32(g_esp + 4), c = dd_mem32(g_esp + 8),
             n = dd_mem32(g_esp + 0xC);
    memset((void *)(uintptr_t)dst, (int)c, (size_t)n);
    g_eax = dst;
}
static void dd_ovr_memcpy(void)
{
    extern uint32_t g_esp;
    extern uint32_t dd_mem32(uint32_t va);
    uint32_t dst = dd_mem32(g_esp + 4), src = dd_mem32(g_esp + 8),
             n = dd_mem32(g_esp + 0xC);
    memmove((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, (size_t)n);
    g_eax = dst;
}

void dd_crt_overrides_register(void)
{
    dd_overrides_add(0x00ECCD14u, dd_ovr_00ECCD14);
    dd_overrides_add(0x00A5B5A0u, dd_ovr_00A5B5A0);
    dd_overrides_add(0x00EC61C0u, dd_ovr_memset);   /* memset do CRT  */
    dd_overrides_add(0x00EC44D0u, dd_ovr_memcpy);   /* memmove        */
    dd_overrides_add(0x00A5B270u, dd_ovr_00A5B270); /* pre-MM: pool header */
    fprintf(stderr, "[DesertDroid] overrides CRT registrados "
            "(00ECCD14, 00A5B5A0, memset/memcpy nativos)\n");
}

/* sub_00AA35A0 = o pool allocator do MM. O boot natural cria o pool via
 * vtable (sub_00AA3430) mas o sub_00AA3760 so inicializa SLOTS, nao o
 * header ([+8]=slot_array, [+0x10]=capacity, [+4]=element_size). Este
 * override completa o header na primeira chamada e chama o original. */
static void dd_ovr_00AA35A0(void)
{
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_ecx;
    extern uint32_t g_eax;
    uint32_t global_pool = dd_mem32(0x11F622Cu);
    if (global_pool >= 0x50000000u && global_pool < 0x68000000u &&
        dd_mem32(global_pool + 8) == 0) {
        *(uint32_t *)(uintptr_t)(global_pool + 8)    = global_pool + 0x80;
        *(uint32_t *)(uintptr_t)(global_pool + 0x10) = 256;
        *(uint32_t *)(uintptr_t)(global_pool + 4)    = 8;
        fprintf(stderr, "[DesertDroid] pool header completado: pool=%#x "
                        "slots=%#x cap=256\n",
                global_pool, global_pool + 0x80);
    }
    /* NAO chamar o original via dispatch — o stub aponta pra CA! Chamar
     * diretamente a funcao liftada via recomp_lookup (bypass do manual). */
    {
        extern recomp_func_t recomp_lookup(uint32_t va);
        recomp_func_t f = recomp_lookup(0x00AA35A0u);
        if (f) {
            f();
        } else {
            fprintf(stderr, "[pool] 00AA35A0: original nao encontrado!\n");
            g_eax = 0;
        }
    }
}

/* sub_00A5B270: chamado ANTES da 1a alocacao do MM. Ponto perfeito para
 * completar o header do pool que o boot natural criou (sub_00AA3760 so
 * inicializa slots, nao o header). */
static void dd_ovr_00A5B270(void)
{
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_ecx;
    uint32_t global_pool = dd_mem32(0x11F622Cu);
    if (global_pool >= 0x50000000u && global_pool < 0x68000000u &&
        dd_mem32(global_pool + 8) == 0) {
        *(uint32_t *)(uintptr_t)(global_pool + 8)    = global_pool + 0x80;
        *(uint32_t *)(uintptr_t)(global_pool + 0x10) = 256;
        *(uint32_t *)(uintptr_t)(global_pool + 4)    = 8;
        fprintf(stderr, "[DesertDroid] pool header (via 00A5B270): "
                        "pool=%#x slots=%#x cap=256\n",
                global_pool, global_pool + 0x80);
    }
    /* chamar o ORIGINAL: 00A5B270 NAO esta em overrides.txt como stub
     * que recursa — e' um stub que chama recomp_lookup_manual, que
     * NOS implementamos. Entao chamar o original via dispatch normal. */
    /* MAS: o dispatch vai achar NOSSO override! Precisamos do ORIGINAL.
     * O stub e' sub_00A5B270 (chamado pela gen code), que chama
     * recomp_lookup_manual(0x00A5B270) que acha DD_OVR_00A5B270 (nos).
     * O ORIGINAL sub_00A5B270 foi SUBSTITUIDO pelo stub. Nao ha original.
     * FIX: fazer o trabalho DO original (format-chain) aqui mesmo. */
    /* O 00A5B270 e' um this-call que faz a lookup na string-table.
     * A cadeia normal: 00A5B270 -> 00A5B4B0 -> 0096AD30 -> 0096AE90
     * O nosso formato responde "" (vazio) — replicar isso. */
    extern uint32_t g_eax;
    extern uint32_t g_esp;
    uint32_t buf = dd_mem32(g_esp + 4);
    if (buf) *(uint32_t*)(uintptr_t)buf = 0;
    g_eax = buf;
    fprintf(stderr, "[DesertDroid] 00A5B270 -> \"\" (buf=%#x)\n", buf);
}
