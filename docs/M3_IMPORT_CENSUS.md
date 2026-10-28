# M3 — Censo de imports (FalloutNV.exe x win32hle)

Fonte: `pcrecomp/tools/pe/extract_imports.py` sobre o exe 1.4.0.525
(sha256 `61de3c74...`) x tabelas de shims do `runtime/win32hle`.

**271 imports no total — 70 já respondidos pelo win32hle — faltam 201.**

Abaixo, os que faltam por DLL, com a estratégia de cada bloco. O bind é
permissivo: o que não estiver implementado vira stub que se anuncia quando
o jogo chama — o bring-up segue a ordem que o boot real pedir, não a ordem
desta lista.

## kernel32.dll (79) — POSIX direto
Threads (Create/Exit/Resume/Suspend/Terminate), critical sections
(Init/Enter/Leave/Delete/Try), TLS (TlsAlloc/Free/GetValue/SetValue),
eventos/semáforos/mutex (Create/Set/Reset/Release/Wait*), arquivos
(FindFirst/Next, Copy/Delete/Move, GetFullPathName, attributes, sizes),
tempo (System/Local/FileTime conversions, QPC), locale/código-página,
Interlocked* (atómicos nativos), VirtualQuery, console (Write/Get*),
DebugBreak (retorno manso), HeapCreate.

**Delta de bring-up (2026-10-28):** `IsDebuggerPresent` já foi adicionado a
`src/host/dd_kernel32.c` e retorna `FALSE`, comportamento esperado sem
 depurador. O próximo smoke boot precisa confirmar que o jogo passa desse
ponto; a contagem acima é o censo-base anterior a esse ajuste.
→ Implementação sobre pthread/POSIX no estilo já usado por kernel32.c.

## d3dx9_38.dll (25) — implementar em C
Math (D3DXMatrix*, D3DXVec*, D3DXPlane*: direto), shader compile/assemble
(D3DXCompileShader*: FNV já embarca shaders pré-compilados — avaliar uso
real no boot), loaders de textura in-memory (D3DXCreateTextureFromFileInMemory
e afins: usados no pipeline de texturas — implementar BC1-BC5 decode já
necessário de qualquer forma p/ Mali).

## user32.dll (27) — win32hle user32.c (brotar)
Window/janela (AdjustWindowRect*, GetClientRect, SetWindowPos, ...),
input (GetAsyncKeyState, SendInput, hooks), mensagens, MessageBox,
cursors/ícones, métricas.

## wsock32.dll (21 ordinals) — stubs
Single-player: sockets não usados. Stubs que retornam erro são suficientes.

## steam_api.dll (11) — stubs no-op (estilo Goldberg)
SteamAPI_Init→true, interfaces→objetos vazios, callbacks→nada. Jogo roda
single-player sem Steam.

## libvorbisfile.dll (6) — libvorbis REAL
ov_* é a API pública da libvorbis de verdade: linkar a biblioteca open
source e responder os imports com ela (decode .ogg nativo).

## ole32.dll (7) — COM mínimo
CoInitialize/CoUninitialize/CoCreateInstance/CoTaskMem* — win32hle já tem
espinha dorsal COM (ver README win32hle).

## winmm.dll (8) — mmio* + timeGetTime
timeGetTime → relógio nativo; mmio* (RIFF I/O em memória — usado por
DirectSound/Miles p/ WAVs) → implementar RIFF parser simples.

## ADVAPI32 (6), SHELL32 (3), COMCTL32 (3), XINPUT1_3 (2 ordinals), DINPUT8 (1), d3d9 (1), DSOUND (1 ordinal)
Registry (advapi32: mapa em arquivo JSON/ini local), SHGetFolderPathA→
pastas Android, ImageList/InitCommonControls→UI da própria engine,
XInput/DInput8→SDL2 gamepad→touch, D3DPERF_SetOptions→no-op,
DirectSoundCreate→nosso backend AAudio, DirectInput8Create→SDL2.

## binkw32.dll (9) — JÁ IMPLEMENTADO
O win32hle já responde os 9 imports de Bink (vídeo!) — os FMVs podem
funcionar cedo no bring-up.
