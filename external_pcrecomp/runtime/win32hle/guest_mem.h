/* guest_mem.h — alocacoes visiveis ao guest em hosts 64-bit (DesertDroid).
 *
 * O modelo RECOMP_FLAT_MEMORY trata o VA de 32 bits do guest como endereco
 * literal do host. Em um host 64-bit, malloc/calloc devolvem ponteiros acima
 * de 4 GB que, truncados ao devolver ao guest, apontam para memoria nao
 * mapeada. Toda memoria que o guest recebe como ponteiro deve sair daqui:
 * uma arena reservada em VA baixo.
 */
#ifndef WIN32HLE_GUEST_MEM_H
#define WIN32HLE_GUEST_MEM_H
#include <stdint.h>
#include <stddef.h>

/* VA de guest de um bloco novo (alinhado a 16, zerado), ou 0 se esgotado. */
uint32_t guest_alloc(size_t size);

/* Realoca: copia min(old,new) para um bloco novo (o tamanho antigo vive no
 * header escondido). Retorna o novo VA ou 0. */
uint32_t guest_realloc(uint32_t old_va, size_t newsize);

/* Liberta (a arena e bump: hoje marca o header e nao reusa; suficiente
 * para o boot — blocos liberados nao voltam, mas nada corrompe). */
void guest_free(uint32_t va);

/* O mesmo, em um VA pedido pelo guest (VirtualAlloc com endereco): mapeia
 * paginas reais no va. Retorna o va ou 0. */
uint32_t guest_alloc_at(uint32_t va, size_t size);

/* VA de guest -> ponteiro host (no modelo flat e o proprio endereco). */
void *guest_ptr(uint32_t va);

/* Bytes ja consumidos do bump (diagnostico de esgotamento). */
uint64_t guest_mem_used(void);

#endif
