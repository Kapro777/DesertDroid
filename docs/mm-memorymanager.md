# FalloutNV MemoryManager — mapa completo (decodificado do exe via capstone)

Toda a arquitetura abaixo foi verificada por disassembly direto do
`FalloutNV.exe` (BYOG, nunca distribuído). Endereços são VAs do guest.

## Singleton e dispatch

```
[0x11F6080] = singleton MM  ->  [singleton] = heap_obj
[heap_obj]   = vtable VA 0x10A24FC (.rdata)
vtable:  [+0x04]=0x00AA30C0  fast path slab (cmd 1)
         [+0x10]=0x00AA3300  command handler alloc (cmd 0xD)
         [+0x14]=0x00AA3370  command handler free  (cmd 0xF)
         [+0x18]=0x00AA33A0  command handler realloc (cmd 0xC)
         [+0x1C]=0x00AA3430  pool constructor
```

O heap_obj natural nao e' construido no nosso caminho de boot —
`dd_crt_override.c` (00ECCD14) monta `container -> heap_obj -> vtable`
e escreve em `[0x11F6118]`. O jogo consome isso ao montar `[0x11F6080]`.

## Wrappers (interface que o jogo chama)

- `0xAA1130 malloc(size, align)`: monta descritor `{align, size, cmd=0xD}`
  na pilha do caller e chama `[vtable+0x10]`. size==0 vira 1.
- `0xAA13E0` (operator new fast entry): descritor com align=4, chama
  `[vtable+0x04]` (0xAA30C0).
- `0xAA11B0(ptr, newsize)`: realloc; com newsize==0 e ptr!=0 = free
  (cmd 0xF via vtable+0x14); ptr==0 + size!=0 = alocacao (cmd 0xC via
  vtable+0x18).

## Command handler alloc (0xAA3300, cmd 0xD)

```
bl = byte[desc] = align
edx = size + align
ecx = 0x11F6238              ; root MM (objeto INLINE em .data)
call 0xAA3E40                ; alocar size+align -> eax = bloco raw
padding = align - (raw & (align-1))
eax = raw + padding          ; ponteiro alinhado
[eax-1] = padding            ; marcador p/ free subtrair
memset(eax, 0, size)         ; 0xEC61C0 (override nativo nosso)
```

## Fast path slab (0xAA30C0 -> 0xAA3810 -> 0xAA35A0)

- So' para align==4, size+4 <= 0x100, tipos != 7/8.
- `0xAA3810`: `slab = pool + (size_header-1)*0x100` onde
  `size_header = size+4` (o 0xAA30C0 soma 4 antes).
- Slab layout (stride 0x100, 256 slabs, pool total 0x10080):
  ```
  [slab+0x00] = class (tamanho do chunk em bytes, = size+4)
  [slab+0x04] = chunks por bloco (BYTE, min(0x6400/class, 255))
  [slab+0x08] = array de entries (8 bytes cada: [0]=bloco,
                [4]=proximo chunk livre (byte), [5]=chunks livres (byte))
  [slab+0x0C] = count usado | [slab+0x10] = capacidade
  [slab+0x14] = free list    | [slab+0x1C] = deferred
  [slab+0x80] = CRITICAL_SECTION
  ```
- `0xAA35A0` (this=slab): EnterCriticalSection(slab+0x80); busca
  entry livre; se nenhuma, cresce array via 0xAA11B0 e aloca bloco
  novo via `malloc(chunks*class)` (0xAA1130); escreve indices 1..chunks
  no bloco; retorna chunk livre ou 0 (slab cheio/nao iniciado).

## Init do pool (0xAA3430 -> 0xAA3760) — COMPLETO por si

```
0xAA3430: pool = 0xAA3E40(0x11F6238, 0x10080)
          0xAA3760(pool, 0x6400)
          [0x11F622C] = pool
0xAA3760: loop1 (256x): zera [slab+0/8/C/10/14/18/1C], CS(slab+0x80)
          [pool+0x10000] = arg (0x6400)
          loop2 (class=1..256): [slab+0]=class
                                 [slab+4]=min(0x6400/class,255)
```

**Nao existe "header faltando"** — teoria antiga (runs #73-87) estava
errada. Os overrides de header fake ([+4]=8) e o stub do 0xAA3300 que
retornava o pool pra toda malloc ERAM a corrupcao.

## Fallback e root

- `0xAA3E40` (this=0x11F6238): `byte[0x11F6238]==0` (nao iniciado)
  -> `0xAA4290` = fallback -> `0xECD1C7(size)` (CRT malloc).
  No nosso boot o root nunca e' iniciado -> TODA alocacao cai no
  fallback CRT. Funciona, e' so' mais lento.
- Path grande (byte!=0) usa TLS: `fs:[0x2C]`, index `[0x126FD98]`,
  `[slot+0x2B4]` — nunca executado no nosso caminho.

## Licoes

1. Nunca fazer override de funcao que nao sabemos o contrato completo.
2. O dump de crash antigo lia `[heap_obj+0x10]` e chamava de "vtable
   parcial" — bug do dump, nao realidade. O dump novo (main.c) le
   `[[heap_obj]]+slots` e o slab[0].
3. `_initterm` termina antes do pool existir; o pool e' criado depois,
   por codigo do jogo — override de 00ECCD14 nao deve tocar no pool.
