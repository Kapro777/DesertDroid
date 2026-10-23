/* DesertDroid — TLS estatico (__declspec(thread)), o que o loader faria.
 *
 * O jogo (Gamebryo/BGS) acessa variaveis __declspec(thread) via:
 *   mov ecx, fs:[0x2C]          ; TEB->ThreadLocalStoragePointer
 *   mov reg, [ecx + index*4]     ; array do modulo
 * O loader do Windows aloca o bloco TLS (template da secao .tls + zero
 * fill), cria o array, escreve o __tls_index do modulo e o ponteiro no
 * TEB, e chama os callbacks TLS. Nos montamos o mesmo cenario.
 *
 * Chamado pelo hook dd_on_tib_ready (patch 0022) quando o TIB da thread
 * e criado — o TEB precisa existir antes de preencher o slot 0x2C.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "win32hle.h"
#include "guest_mem.h"

#define IMAGE_BASE 0x00400000u
#define TLS_DIR_VA (IMAGE_BASE + 0x00D0147Cu)   /* do PE: data dir[9] RVA 0xD0147C */

typedef struct {
    uint32_t start_va, end_va, index_va, callbacks_va;
    uint32_t zero_fill, characteristics;
} IMAGE_TLS_DIRECTORY32_G;

void dd_on_tib_ready(uint32_t tib_va)
{
    IMAGE_TLS_DIRECTORY32_G *tls = (IMAGE_TLS_DIRECTORY32_G *)(uintptr_t)TLS_DIR_VA;
    if (!tls->start_va || !tls->end_va) {
        fprintf(stderr, "[tls-est] sem diretorio TLS (start=%#x)\n", tls->start_va);
        return;                                  /* exe sem TLS: nada a fazer */
    }
    uint32_t raw_size = tls->end_va - tls->start_va;
    uint32_t block_size = raw_size + tls->zero_fill;
    uint32_t block = guest_alloc(block_size ? block_size : 4);
    memcpy(guest_ptr(block), (void *)(uintptr_t)tls->start_va, raw_size);
    /* NOTA: nao preencher slots do jogo aqui — o A5B4B0 trata pointer
     * NULL graciosamente (ret -1); objeto zerado nao-NULL e que crasha. */
    /* array de TLS: 64 slots; o exe e o primeiro modulo -> indice 0 */
    uint32_t array = guest_alloc(4u * 64u);
    memset(guest_ptr(array), 0, 4u * 64u);
    *(uint32_t *)guest_ptr(array + 4u * 0) = block;   /* slot[0] = bloco do exe */
    if (tls->index_va)
        *(uint32_t *)(uintptr_t)tls->index_va = 0;    /* __tls_index = 0 */
    *(uint32_t *)guest_ptr(tib_va + 0x2Cu) = array;   /* TEB->ThreadLocalStoragePointer */
    fprintf(stderr, "[tls-est] bloco %#x (%u bytes), array %#x, TIB %#x slot 0x2C ok\n",
            block, block_size, array, tib_va);
    /* callbacks TLS (DLL_PROCESS_ATTACH = 1) */
    if (tls->callbacks_va) {
        uint32_t cb;
        uint32_t i = 0;
        while ((cb = *(uint32_t *)(uintptr_t)(tls->callbacks_va + 4 * i)) != 0) {
            fprintf(stderr, "[tls-est] callback TLS sub_%08X(DLL_PROCESS_ATTACH)\n", cb);
            {
                const uint32_t args[3] = { IMAGE_BASE, 1u, 0u };
                hle_call_guest(cb, 3, args);
            }
            i++;
        }
    }
}
