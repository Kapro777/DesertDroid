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
static void dd_ovr_00A5B270(void);
static void dd_ovr_00A5B690(void);
static void dd_ovr_00AA30C0(void);
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
    /* heap Havok: [0x11F6118] = container -> heap_obj (vtable real).
     * O init natural do heap nao roda no nosso caminho de boot; o jogo
     * le [0x11F6118] ao montar o singleton MM ([0x11F6080]) e despacha
     * os command handlers (alloc cmd 0xD, free 0xF, realloc 0xC) pela
     * vtable REAL em 0x10A24FC — tudo codigo liftado do jogo.
     * NAO mexer no pool ([0x11F622C]): o init natural (sub_00AA3430 ->
     * sub_00AA3760) ja escreve [slab+0]=class e [slab+4]=chunks/blk. */
    {
        extern uint32_t dd_mem32(uint32_t va);
        extern uint32_t guest_alloc(size_t size);
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
    /* NAO mexer no pool MM ([0x11F622C]): o init natural
     * (sub_00AA3430 -> sub_00AA3760) ja inicializa todos os slabs. */
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
    dd_overrides_add(0x00A5B270u, dd_ovr_00A5B270); /* format-chain: "" */
    dd_overrides_add(0x00A5B690u, dd_ovr_00A5B690); /* strlen: string vazia */
    dd_overrides_add(0x00AA30C0u, dd_ovr_00AA30C0); /* fast path MM -> geral */
    fprintf(stderr, "[DesertDroid] overrides CRT registrados "
            "(00ECCD14, 00A5B5A0, memset/memcpy nativos)\n");
}

/* sub_00AA30C0 (vtable+0x04 do MM): fast path de alocacao via slabs do
 * pool ([0x11F622C]). O scan do 0xAA35A0 le [slab+8]/[slab+0xC] e cruza
 * para pagina NULL (crash 0x10) ou livelock (run #91 sob GDB). Override:
 * roteia TUDO para o path geral PROVADO (0xAA1130 -> cmd 0xD -> 0xAA3300
 * real -> 0xAA3E40 -> fallback CRT 0xECD1C7 -> HeapAlloc), replicando as
 * duas convencoes de bloco do original:
 *   - elegivel ao fast path (align==4, arg4==0, tipo!=7/8, size+4<=0x100):
 *     aloca size+4, escreve header de size, retorna bloco+4 (bl=1)
 *   - caso contrario: aloca size, retorna bloco cru (bl=0)
 * Free/realloc nao sao afetados: roteiam pelo range do pool (nunca batem)
 * e caem no fallback CRT — verificado no disassembly do 0xAA3370/0xAA33A0. */
static void dd_ovr_00AA30C0(void)
{
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_esp;
    uint32_t size_ptr = dd_mem32(g_esp + 4);
    uint32_t desc_ptr = dd_mem32(g_esp + 8);
    uint32_t type     = dd_mem32(g_esp + 0xC);
    uint32_t arg4     = dd_mem32(g_esp + 0x10);
    uint32_t size  = dd_mem32(size_ptr);
    uint32_t align = dd_mem32(desc_ptr);
    if (align == 4u && arg4 == 0u && type != 7u && type != 8u &&
        size + 4u <= 0x100u) {
        *(uint32_t *)(uintptr_t)size_ptr = size + 4u;  /* side-effect original */
        uint32_t args[2] = { size + 4u, 4u };
        uint32_t blk = hle_call_guest(0x00AA1130u, 2, args);
        if (blk == 0u) {
            g_eax = 0u;
        } else {
            *(uint32_t *)(uintptr_t)blk = size + 4u;    /* header de size  */
            g_eax = blk + 4u;                           /* usuario = blk+4 */
        }
        {
            static uint32_t n = 0;
            if (n < 3)
                fprintf(stderr, "[DesertDroid] 00AA30C0 bypass: size=%u blk=%#x\n",
                        size + 4u, blk);
            n++;
        }
    } else {
        uint32_t args[2] = { size, align };
        g_eax = hle_call_guest(0x00AA1130u, 2, args);  /* bloco cru */
        {
            static uint32_t n = 0;
            if (n < 3)
                fprintf(stderr, "[DesertDroid] 00AA30C0 bypass geral: size=%u align=%u -> %#x\n",
                        size, align, g_eax);
            n++;
        }
    }
    g_esp += 4u + 0x1Cu;   /* ret 0x1C: pop ret + 7 args do thiscall do jogo */
}

/* sub_00A5B270: elo da cadeia format-string (00A5B270 -> 00A5B4B0 ->
 * 0096AD30 -> 0096AE90). Sem o string-manager carregado (init que nao
 * roda no nosso caminho), o original crasha na lookup. Resposta util:
 * string vazia no buffer + buffer como retorno (mesmo contrato do
 * 00A5B5A0). NAO mexer no pool MM — o init natural dele e completo. */
static void dd_ovr_00A5B270(void)
{
    extern uint32_t g_eax;
    extern uint32_t g_esp;
    extern uint32_t dd_mem32(uint32_t va);
    uint32_t buf = dd_mem32(g_esp + 4);
    if (buf) *(uint32_t*)(uintptr_t)buf = 0;
    g_eax = buf;
    fprintf(stderr, "[DesertDroid] 00A5B270 -> \"\" (buf=%#x)\n", buf);
}

/* sub_00A5B690: strlen/string lookup do jogo. Le [0x11F42C0]+0x2080
 * como tabela de strings e faz lookup por ID. Sem a tabela carregada
 * (o jogo carrega de arquivos), responde strlen=0 (string vazia). */
static void dd_ovr_00A5B690(void)
{
    extern uint32_t g_eax;
    g_eax = 0;   /* strlen = 0: string vazia */
}
