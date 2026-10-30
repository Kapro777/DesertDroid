/* DesertDroid — stubs de importação crítica para o caminho de boot.
 *
 * O jogo importa ~154 símbolos que ainda não implementamos. Em vez de um
 * round de CI por import, registramos aqui stubs "fail-clean" que devolvem
 * valores úteis o suficiente para o jogo criar janela e iniciar:
 *   - handles     -> não-NULL (pseudo-handle fixo)
 *   - BOOL        -> TRUE
 *   - HRESULT     -> S_OK (0)
 *   - strings     -> ponteiro para string vazia na arena
 *   - interfaces  -> ponteiro para objeto simulado na arena (vtable zero)
 *
 * Nada disso toca em arquivos de jogo (BYOG). Se o jogo precisar de algo
 * real, o próximo crash mostrará exatamente qual.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "win32hle.h"

/* ---------- KERNEL32 ---------- */

/* CreateFileW(...) -> HANDLE */
static void k_CreateFileW(void) {
    extern uint32_t g_eax;
    g_eax = 0x00000001u;   /* pseudo-handle não-NULL */
    RET(1, 7);
}

/* GetCommandLineW() -> LPWSTR. O ponteiro tem que pertencer à arena guest. */
static void k_GetCommandLineW(void) {
    extern uint32_t guest_alloc(size_t);
    extern void *guest_ptr(uint32_t);
    static const char cmdline[] = "C:\\FalloutNV\\FalloutNV.exe";
    uint32_t va = guest_alloc(sizeof cmdline * sizeof(uint16_t));
    uint16_t *wide = (uint16_t *)guest_ptr(va);
    for (size_t i = 0; i < sizeof cmdline; i++)
        wide[i] = (uint8_t)cmdline[i];
    RETP(va, 0);
}

/* GetFileSizeEx(hFile, lpFileSize) -> BOOL */
static void k_GetFileSizeEx(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 2);
}

/* VirtualQuery(lpAddress, lpBuffer, dwLength) -> SIZE_T */
static void k_VirtualQuery(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 3);
}

/* GetCommandLineA() -> LPSTR. Devolve argv[0] virtual, nunca NULL. */
static void k_GetCommandLineA(void) {
    extern uint32_t guest_alloc(size_t);
    extern void *guest_ptr(uint32_t);
    static const char cmdline[] = "C:\\FalloutNV\\FalloutNV.exe";
    uint32_t va = guest_alloc(sizeof cmdline);
    memcpy(guest_ptr(va), cmdline, sizeof cmdline);
    RETP(va, 0);
}

/* GetFullPathNameA(...) -> DWORD */
static void k_GetFullPathNameA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 4);
}

/* FreeEnvironmentStringsA(lpsz) -> BOOL */
static void k_FreeEnvironmentStringsA(void) {
    RET(1, 1);
}

/* ---------- USER32 ---------- */

/* RegisterClassA(...) -> ATOM */
static void k_RegisterClassA(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 1);
}

/* CreateWindowExA(...) -> HWND */
static void k_CreateWindowExA(void) {
    extern uint32_t g_eax;
    g_eax = 0x00010000u;
    RET(1, 12);
}

/* DefWindowProcA(...) -> LRESULT */
static void k_DefWindowProcA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 4);
}

/* PeekMessageA(...) -> BOOL */
static void k_PeekMessageA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(1, 5);
}

/* GetMessageA(...) -> BOOL */
static void k_GetMessageA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(1, 4);
}

/* DispatchMessageA(...) -> LRESULT */
static void k_DispatchMessageA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(1, 1);
}

/* TranslateMessage(...) -> BOOL */
static void k_TranslateMessage(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 1);
}

/* ShowWindow(...) -> BOOL */
static void k_ShowWindow(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 2);
}

/* UpdateWindow(...) -> BOOL */
static void k_UpdateWindow(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 1);
}

/* GetClientRect(hWnd, lpRect) -> BOOL */
static void k_GetClientRect(void) {
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_eax;
    uint32_t rc = dd_mem32(g_esp + 4);
    if (rc) memset((void *)(uintptr_t)rc, 0, 16);
    g_eax = 1u;
    RET(1, 2);
}

/* GetSystemMetrics(nIndex) -> int
 *
 * run 40992593558: o boot MORRIA antes do D3D9 sem crash algum — WinMain,
 * logo apos o mutex de instancia unica, consulta SM_REMOTESESSION (0x1000);
 * o stub devolvia 1 para QUALQUER indice (o RET(1,1) sobrescrevia o
 * g_eax=800) -> teste != 0 = "sessao remota" -> MessageBoxA (texto em
 * falloutnv.exe+0xC82A48: recusa de remote desktop) -> exit(0) antes de
 * criar qualquer coisa grafica.
 *
 * Agora o indice importa. Caminhos que consultamos no boot:
 *   - 0x1000 SM_REMOTESESSION -> 0 (sessao local; CI roda em container)
 *   - 80 SM_CMONITORS -> 1 (um monitor: o dialogo de multi-monitor do
 *     Gamebryo tela a saida tambem)
 *   - 0 SM_CXSCREEN / 1 SM_CYSCREEN / 16/17 fullscreen -> 1920x1080
 * O restante devolve 0 (neutro: checks "if (metric)" leem ausencia, que
 * e a verdade no nosso host). Log one-shot por indice (cap 16) para a
 * proxima boot mostrar o que mais o jogo perguntar. */
static void k_GetSystemMetrics(void) {
    extern uint32_t g_esp;
    extern uint32_t dd_mem32(uint32_t va);
    uint32_t n = dd_mem32(g_esp + 4);
    uint32_t r = 0;
    switch (n) {
    case 0:      r = 1920; break;   /* SM_CXSCREEN    */
    case 1:      r = 1080; break;   /* SM_CYSCREEN    */
    case 16:     r = 1920; break;   /* SM_CXFULLSCREEN */
    case 17:     r = 1080; break;   /* SM_CYFULLSCREEN */
    case 80:     r = 1;    break;   /* SM_CMONITORS   */
    case 81:     r = 1;    break;   /* SM_SAMEDISPLAYFORMAT */
    case 0x1000: r = 0;    break;   /* SM_REMOTESESSION: sessao local */
    case 19:     r = 1;    break;   /* SM_MOUSEPRESENT */
    default:     r = 0;    break;
    }
    {
        static uint32_t seen[16]; static int seen_n;
        int dup = 0;
        for (int i = 0; i < seen_n; i++) if (seen[i] == n) dup = 1;
        if (!dup && seen_n < 16) { seen[seen_n++] = n;
            fprintf(stderr, "[DesertDroid] GetSystemMetrics(%u) -> %u\n", n, r); }
    }
    RET(r, 1);
}

/* LoadIconA(...) -> HICON */
static void k_LoadIconA(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 2);
}

/* LoadCursorA(...) -> HCURSOR */
static void k_LoadCursorA(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 2);
}

/* MessageBoxA(...) -> int */
static void k_MessageBoxA(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 4);
}

/* SendMessageA(...) -> LRESULT */
static void k_SendMessageA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 4);
}

/* PostQuitMessage(nExitCode) -> void */
static void k_PostQuitMessage(void) {
    RETV(1);
}

/* AdjustWindowRectEx(...) -> BOOL */
static void k_AdjustWindowRectEx(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 4);
}

/* FindWindowA(...) -> HWND */
static void k_FindWindowA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 2);
}

/* GetWindowLongA(...) -> LONG */
static void k_GetWindowLongA(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 2);
}

/* SetWindowPos(...) -> BOOL */
static void k_SetWindowPos(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 7);
}

/* SetForegroundWindow(...) -> BOOL */
static void k_SetForegroundWindow(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 1);
}

/* SetWindowTextA(...) -> BOOL */
static void k_SetWindowTextA(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 2);
}

/* GetClassNameA(...) -> int */
static void k_GetClassNameA(void) {
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_eax;
    uint32_t buf = dd_mem32(g_esp + 4);
    if (buf) memset((void *)(uintptr_t)buf, 0, 16);
    g_eax = 0u;
    RET(0, 3);
}

/* ---------- STEAM API ---------- */

static void s_SteamAPI_Init(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 0);
}

static void s_SteamAPI_IsSteamRunning(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(1, 0);
}

static void s_SteamAPI_Shutdown(void) { RETV(0); }
static void s_SteamAPI_RunCallbacks(void) { RETV(0); }

static void steam_obj(void) {
    extern uint32_t g_eax;
    extern uint32_t guest_alloc(size_t);
    extern void *guest_ptr(uint32_t);
    uint32_t obj = guest_alloc(64);
    memset(guest_ptr(obj), 0, 64);
    g_eax = obj;
    RET(0, 0);
}

static void s_SteamApps(void)           { steam_obj(); }
static void s_SteamUser(void)           { steam_obj(); }
static void s_SteamFriends(void)        { steam_obj(); }
static void s_SteamUserStats(void)      { steam_obj(); }
static void s_SteamUtils(void)          { steam_obj(); }

static void s_SteamAPI_RegisterCallback(void) { RETV(2); }

/* ---------- D3D9 ---------- */

/* Direct3DCreate9 mora no dd_d3d9.c (COM HLE auto-anunciante, vtable real).
 * Um stub aqui sobrescreveria o registro de la (newest-wins) com um objeto
 * zerado — vtable NULL = crash na primeira chamada de metodo do guest. */

/* ---------- OLE32 ---------- */

static void o_CoInitialize(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 1);
}

static void o_CoInitializeEx(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 2);
}

static void o_CoUninitialize(void) { RETV(1); }

static void o_CoCreateInstance(void) {
    extern uint32_t g_eax;
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t guest_alloc(size_t);
    extern void *guest_ptr(uint32_t);
    uint32_t ppv = dd_mem32(g_esp + 0x10);
    if (ppv) {
        uint32_t obj = guest_alloc(256);
        memset(guest_ptr(obj), 0, 256);
        *(uint32_t *)(uintptr_t)ppv = obj;
    }
    g_eax = 0u;
    RET(0, 5);
}

static void o_CoTaskMemAlloc(void) {
    extern uint32_t guest_alloc(size_t);
    extern uint32_t g_eax;
    g_eax = guest_alloc(A32(0));
    RET(1, 1);
}

static void o_CoTaskMemFree(void) { RETV(1); }

/* ---------- WINMM mmio* ---------- */
static void w_mmioOpenA(void)   { RET(0, 3); }
static void w_mmioClose(void)   { RETV(1); }
static void w_mmioRead(void)    { RET(0, 3); }
static void w_mmioAdvance(void) { RETV(1); }
static void w_mmioAscend(void)  { RETV(1); }
static void w_mmioDescend(void) { RETV(1); }
static void w_mmioGetInfo(void) { RET(0, 2); }

/* ---------- SHELL32 ---------- */
static void sh_CommandLineToArgvW(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 2);
}

static void sh_ShellExecuteA(void) {
    extern uint32_t g_eax;
    g_eax = 1u;
    RET(0, 6);
}

/* ---------- XINPUT1_3 (ordinals) ---------- */
static void x_XInput1_3_2(void) { RET(0, 4); }
static void x_XInput1_3_3(void) { RET(0, 4); }

/* ---------- DINPUT8 ---------- */
static void d8_DirectInput8Create(void) {
    extern uint32_t g_eax;
    g_eax = 0u;
    RET(0, 5);
}

/* ---------- DSOUND ---------- */
static void ds_DSOUND_11(void) { RET(0, 4); }

/* ---------- COMCTL32 ---------- */
static void cc_ImageList_Destroy(void)    { RETV(1); }
static void cc_ImageList_LoadImageA(void) { RET(0, 7); }
static void cc_InitCommonControlsEx(void) { RET(1, 1); }

/* ---------- libvorbisfile ---------- */
static void v_ov_clear(void)         { RETV(1); }
static void v_ov_info(void)          { RET(0, 1); }
static void v_ov_open_callbacks(void){ RET(0, 4); }
static void v_ov_pcm_total(void)     { RET(0, 1); }
static void v_ov_read(void)          { RET(0, 6); }
static void v_ov_seekable(void)      { RET(0, 1); }

/* ---------- USER32 (caminho do WinMain: janela, teclado, enumeração) ----------
 * Todos faltavam: import sem shim = hle_unimplemented -> hle_fatal (aborta).
 * Sequência do WinMain após o GetSystemMetrics: GetActiveWindow@0x86B343,
 * AdjustWindowRect@0x86AF0C, GetWindow, hooks de teclado (GetAsyncKeyState...). */
static void u_GetWindow(void)              { RET(0, 2); }   /* NULL */
static void u_ShowCursor(void)             { RET(1, 1); }
static void u_GetDoubleClickTime(void)     { RET(500, 0); }
static void u_SwapMouseButton(void)        { RET(1, 1); }
static void u_UnhookWindowsHookEx(void)    { RET(1, 1); }
static void u_SetWindowsHookExA(void)      { RET(0, 4); }   /* NULL: sem hook */
static void u_CallNextHookEx(void)         { RET(0, 4); }
static void u_EnumChildWindows(void)       { RET(1, 3); }   /* TRUE: nenhuma filha */
static void u_GetActiveWindow(void)        { RET(0, 0); }
static void u_GetAsyncKeyState(void)       { RET(0, 1); }
static void u_GetClassLongA(void)          { RET(0, 2); }
static void u_EnumDisplayDevicesA(void)    { RET(0, 4); }   /* FALSE: só o default */
static void u_AdjustWindowRect(void)       { RET(1, 3); }   /* TRUE: sem ajuste */
static void u_SendInput(void)              { RET(0, 3); }
static void u_GetWindowTextA(void)
{
    uint32_t buf = APTR(1);
    if (buf) MEM8(buf) = 0;
    RET(0, 3);
}

/* ---------- KERNEL32 (tempo/console/debug — faltavam no censo) ---------- */
static void k2_CompareFileTime(void)       { RET(0, 2); }   /* iguais */
static void k2_FormatMessageA(void)        { uint32_t b = APTR(4);
                                              if (b) MEM8(b) = 0; RET(0, 6); }
static void k2_FileTimeToSystemTime(void)  { void *st = (void *)APTR(1);
                                              if (st) memset(st, 0, 16); RET(1, 2); }
static void k2_FileTimeToLocalFileTime(void){ uint32_t l = APTR(1);
                                              if (l) MEM64(l) = MEM64(APTR(0)); RET(1, 2); }
static void k2_SystemTimeToTzSpecificLocalTime(void)
{
    uint32_t lst = APTR(2);
    if (lst) for (int i = 0; i < 4; i++) MEM32(lst + 4 * i) = MEM32(APTR(1) + 4 * i);
    RET(1, 3);
}
static void k2_DebugBreak(void)            { RETV(0); }
static void k2_SetFilePointerEx(void)      { uint32_t nw = APTR(2);
                                              if (nw) MEM64(nw) = 0; RET(1, 4); }
static void k2_WriteConsoleA(void)         { uint32_t w = APTR(3);
                                              if (w) MEM32(w) = A32(2); RET(1, 5); }
static void k2_WriteConsoleW(void)         { uint32_t w = APTR(3);
                                              if (w) MEM32(w) = A32(2); RET(1, 5); }

/* ---------- binkw32 (vídeos de logo: falha limpa = NULL/void) ---------- */
static void b_BinkOpen(void)               { RET(0, 2); }
static void b_BinkOpenDirectSound(void)    { RET(0, 1); }
static void b_BinkClose(void)              { RETV(1); }
static void b_BinkDoFrame(void)            { RETV(1); }
static void b_BinkNextFrame(void)          { RETV(1); }
static void b_BinkWait(void)               { RETV(1); }
static void b_BinkPause(void)              { RETV(2); }
static void b_BinkSetSoundSystem(void)     { RETV(2); }
static void b_BinkCopyToBufferRect(void)   { RETV(11); }

/* ---------- d3d9.dll ---------- */
static void p_D3DPERF_SetOptions(void)     { RETV(1); }

/* ---------- d3dx9_38 (faltantes + argc correto nos já registrados) ---------- */
#define D3DERR_INVALIDCALL 0x88760866u
#define DX_E_FAIL         0x80004005u
/* criação/carregamento: falha explícita (o jogo trata) */
static void dx_D3DXLoadSurfaceFromSurface(void)          { RET(DX_E_FAIL, 5); }
static void dx_D3DXSaveTextureToFileA(void)              { RET(DX_E_FAIL, 3); }
static void dx_D3DXAssembleShaderFromFileA(void)         { RET(DX_E_FAIL, 6); }
static void dx_D3DXCompileShaderFromFileA(void)          { RET(DX_E_FAIL, 11); }
static void dx_D3DXGetShaderConstantTable(void)          { RET(DX_E_FAIL, 2); }
static void dx_D3DXGetImageInfoFromFileInMemory(void)    { RET(DX_E_FAIL, 4); }
static void dx_D3DXCreateCubeTextureFromFileInMemory(void)   { RET(D3DERR_INVALIDCALL, 6); }
static void dx_D3DXCreateVolumeTextureFromFileInMemory(void){ RET(D3DERR_INVALIDCALL, 6); }
/* perfil de GPU moderna fingida (o jogo segue o caminho de shaders) */
static uint32_t arena_cstr(const char *s)
{
    extern uint32_t guest_alloc(size_t);
    extern void *guest_ptr(uint32_t);
    size_t n = strlen(s) + 1;
    uint32_t va = guest_alloc((n + 3) & ~3u);
    memcpy(guest_ptr(va), s, n);
    return va;
}
static void dx_D3DXGetVertexShaderProfile(void)          { RETP(arena_cstr("vs_3_0"), 0); }
static void dx_D3DXGetPixelShaderProfile(void)           { RETP(arena_cstr("ps_3_0"), 0); }
/* D3DXMATRIX = 16 floats row-major (m[4][4]); vetores = 3/4 floats */
static void dx_D3DXMatrixTranspose(void)                 /* (out, m) */
{
    float *o = (float *)APTR(0), *m = (float *)APTR(1);
    if (o && m) for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++)
        o[r * 4 + c] = m[c * 4 + r];
    RET(0, 2);
}
static void dx_D3DXMatrixMultiplyTranspose(void)         /* (out, a, b): (a*b)^T */
{
    float *o = (float *)APTR(0), *a = (float *)APTR(1), *b = (float *)APTR(2);
    if (o && a && b) for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        float s = 0; for (int k = 0; k < 4; k++) s += a[r * 4 + k] * b[k * 4 + c];
        o[c * 4 + r] = s;
    }
    RET(0, 3);
}
static void dx_D3DXVec4Transform(void)                   /* (out, in, m) */
{
    float *o = (float *)APTR(0), *i = (float *)APTR(1), *m = (float *)APTR(2);
    if (o && i && m) for (int r = 0; r < 4; r++)
        o[r] = m[r*4+0]*i[0] + m[r*4+1]*i[1] + m[r*4+2]*i[2] + m[r*4+3]*i[3];
    RET(0, 3);
}
static void dx_D3DXVec3TransformCoord(void)              /* (out, in, m) + translação */
{
    float *o = (float *)APTR(0), *i = (float *)APTR(1), *m = (float *)APTR(2);
    if (o && i && m) for (int r = 0; r < 3; r++)
        o[r] = m[r*4+0]*i[0] + m[r*4+1]*i[1] + m[r*4+2]*i[2] + m[r*4+3];
    RET(0, 3);
}
static void dx_D3DXVec3TransformNormal(void)             /* sem translação */
{
    float *o = (float *)APTR(0), *i = (float *)APTR(1), *m = (float *)APTR(2);
    if (o && i && m) for (int r = 0; r < 3; r++)
        o[r] = m[r*4+0]*i[0] + m[r*4+1]*i[1] + m[r*4+2]*i[2];
    RET(0, 3);
}
static void dx_D3DXPlaneTransform(void)                  /* (out, p, m): transposta */
{
    float *o = (float *)APTR(0), *p = (float *)APTR(1), *m = (float *)APTR(2);
    if (o && p && m) for (int r = 0; r < 4; r++) {
        float t = p[3];
        for (int c = 0; c < 3; c++) t += m[c * 4 + r] * p[c];
        o[r] = t;
    }
    RET(0, 3);
}
static void dx_D3DXPlaneNormalize(void)                  /* (out, p) */
{
    float *o = (float *)APTR(0), *p = (float *)APTR(1);
    if (o && p) {
        float n = sqrtf(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
        if (n > 0.0f) { for (int i = 0; i < 3; i++) o[i] = p[i] / n; o[3] = p[3] / n; }
        else memcpy(o, p, 16);
    }
    RET(0, 2);
}
/* argc correto nos que já existiam (dx_stum genérico com 4 desalinhava a pilha) */
static void dx_D3DXAssembleShader(void)                  { RET(0, 6); }
static void dx_D3DXCompileShader(void)                   { RET(0, 9); }
static void dx_D3DXCreateBuffer(void)                    { RET(0, 1); }
static void dx_D3DXCreateTextureFromFileInMemory(void)   { RET(0, 4); }
static void dx_D3DXMatrixInverse(void)                   { RET(0, 3); }
static void dx_D3DXMatrixMultiply(void)                  { RET(0, 3); }
static void dx_D3DXMatrixRotationYawPitchRoll(void)      { RET(0, 4); }
static void dx_D3DXVec3Normalize(void)                   { RET(0, 2); }

/* ---------- ole32 / steam_api (faltantes) ---------- */
static void o_CoFreeUnusedLibraries(void)                { RETV(0); }
static void s_SteamAPI_UnregisterCallback(void)          { RETV(1); }

/* ---------- ADVAPI32 ---------- */
static void a_RegCloseKey(void)         { RET(0, 1); }
static void a_RegCreateKeyExA(void)     { RET(0, 9); }
static void a_RegOpenKeyExA(void)       { RET(0, 5); }
static void a_RegQueryValueExA(void)    { RET(0, 6); }
static void a_RegSetValueExA(void)      { RET(0, 6); }

/* Tabela de shims — compatível com win32hle_register() */
static const win32hle_shim stubs_boot[] = {
    /* KERNEL32 */
    { "CreateFileW",            (recomp_func_t)k_CreateFileW },
    { "GetCommandLineW",        (recomp_func_t)k_GetCommandLineW },
    { "GetFileSizeEx",          (recomp_func_t)k_GetFileSizeEx },
    { "VirtualQuery",           (recomp_func_t)k_VirtualQuery },
    { "GetCommandLineA",        (recomp_func_t)k_GetCommandLineA },
    { "GetFullPathNameA",       (recomp_func_t)k_GetFullPathNameA },
    { "FreeEnvironmentStringsA",(recomp_func_t)k_FreeEnvironmentStringsA },

    /* USER32 */
    { "RegisterClassA",         (recomp_func_t)k_RegisterClassA },
    { "CreateWindowExA",        (recomp_func_t)k_CreateWindowExA },
    { "DefWindowProcA",         (recomp_func_t)k_DefWindowProcA },
    { "PeekMessageA",           (recomp_func_t)k_PeekMessageA },
    { "GetMessageA",            (recomp_func_t)k_GetMessageA },
    { "DispatchMessageA",       (recomp_func_t)k_DispatchMessageA },
    { "TranslateMessage",       (recomp_func_t)k_TranslateMessage },
    { "ShowWindow",             (recomp_func_t)k_ShowWindow },
    { "UpdateWindow",           (recomp_func_t)k_UpdateWindow },
    { "GetClientRect",          (recomp_func_t)k_GetClientRect },
    { "GetSystemMetrics",       (recomp_func_t)k_GetSystemMetrics },
    { "LoadIconA",              (recomp_func_t)k_LoadIconA },
    { "LoadCursorA",            (recomp_func_t)k_LoadCursorA },
    { "MessageBoxA",            (recomp_func_t)k_MessageBoxA },
    { "SendMessageA",           (recomp_func_t)k_SendMessageA },
    { "PostQuitMessage",        (recomp_func_t)k_PostQuitMessage },
    { "AdjustWindowRectEx",     (recomp_func_t)k_AdjustWindowRectEx },
    { "FindWindowA",            (recomp_func_t)k_FindWindowA },
    { "GetWindowLongA",         (recomp_func_t)k_GetWindowLongA },
    { "SetWindowPos",           (recomp_func_t)k_SetWindowPos },
    { "SetForegroundWindow",    (recomp_func_t)k_SetForegroundWindow },
    { "SetWindowTextA",         (recomp_func_t)k_SetWindowTextA },
    { "GetClassNameA",          (recomp_func_t)k_GetClassNameA },

    /* STEAM */
    { "SteamAPI_Init",             (recomp_func_t)s_SteamAPI_Init },
    { "SteamAPI_IsSteamRunning",   (recomp_func_t)s_SteamAPI_IsSteamRunning },
    { "SteamAPI_Shutdown",         (recomp_func_t)s_SteamAPI_Shutdown },
    { "SteamAPI_RunCallbacks",     (recomp_func_t)s_SteamAPI_RunCallbacks },
    { "SteamApps",                 (recomp_func_t)s_SteamApps },
    { "SteamUser",                 (recomp_func_t)s_SteamUser },
    { "SteamFriends",              (recomp_func_t)s_SteamFriends },
    { "SteamUserStats",            (recomp_func_t)s_SteamUserStats },
    { "SteamUtils",                (recomp_func_t)s_SteamUtils },
    { "SteamAPI_RegisterCallback", (recomp_func_t)s_SteamAPI_RegisterCallback },

    /* OLE32 */
    /* (Direct3DCreate9 vem do dd_d3d9.c: COM HLE com vtable real —
     * um stub zerado aqui sobrescreveria o registro de la, newest-wins.) */
    { "CoInitialize",             (recomp_func_t)o_CoInitialize },
    { "CoInitializeEx",           (recomp_func_t)o_CoInitializeEx },
    { "CoUninitialize",           (recomp_func_t)o_CoUninitialize },
    { "CoCreateInstance",         (recomp_func_t)o_CoCreateInstance },
    { "CoTaskMemAlloc",           (recomp_func_t)o_CoTaskMemAlloc },
    { "CoTaskMemFree",            (recomp_func_t)o_CoTaskMemFree },

    /* WINMM */
    { "mmioOpenA",                (recomp_func_t)w_mmioOpenA },
    { "mmioClose",                (recomp_func_t)w_mmioClose },
    { "mmioRead",                 (recomp_func_t)w_mmioRead },
    { "mmioAdvance",              (recomp_func_t)w_mmioAdvance },
    { "mmioAscend",               (recomp_func_t)w_mmioAscend },
    { "mmioDescend",              (recomp_func_t)w_mmioDescend },
    { "mmioGetInfo",              (recomp_func_t)w_mmioGetInfo },

    /* SHELL32 */
    { "CommandLineToArgvW",       (recomp_func_t)sh_CommandLineToArgvW },
    { "ShellExecuteA",            (recomp_func_t)sh_ShellExecuteA },

    /* XINPUT1_3 (ordinal keys — o pe_loader resolve "DLL#N") */
    { "XINPUT1_3#2",              (recomp_func_t)x_XInput1_3_2 },
    { "XINPUT1_3#3",              (recomp_func_t)x_XInput1_3_3 },

    /* DINPUT8 */
    { "DirectInput8Create",       (recomp_func_t)d8_DirectInput8Create },

    /* DSOUND */
    { "DSOUND#11",                (recomp_func_t)ds_DSOUND_11 },

    /* COMCTL32 */
    { "ImageList_Destroy",        (recomp_func_t)cc_ImageList_Destroy },
    { "ImageList_LoadImageA",     (recomp_func_t)cc_ImageList_LoadImageA },
    { "InitCommonControlsEx",     (recomp_func_t)cc_InitCommonControlsEx },

    /* libvorbisfile */
    { "ov_clear",                 (recomp_func_t)v_ov_clear },
    { "ov_info",                  (recomp_func_t)v_ov_info },
    { "ov_open_callbacks",        (recomp_func_t)v_ov_open_callbacks },
    { "ov_pcm_total",             (recomp_func_t)v_ov_pcm_total },
    { "ov_read",                  (recomp_func_t)v_ov_read },
    { "ov_seekable",              (recomp_func_t)v_ov_seekable },

    /* ADVAPI32 */
    { "RegCloseKey",              (recomp_func_t)a_RegCloseKey },
    { "RegCreateKeyExA",          (recomp_func_t)a_RegCreateKeyExA },
    { "RegOpenKeyExA",            (recomp_func_t)a_RegOpenKeyExA },
    { "RegQueryValueExA",         (recomp_func_t)a_RegQueryValueExA },
    { "RegSetValueExA",           (recomp_func_t)a_RegSetValueExA },

    /* d3dx9_38 — os que existiam, agora com argc correto (o dx_stub genérico
     * com 4 desalinharia a pilha); as transformações são implementadas */
    { "D3DXVec3TransformCoord",         (recomp_func_t)dx_D3DXVec3TransformCoord },
    { "D3DXVec3TransformNormal",  (recomp_func_t)dx_D3DXVec3TransformNormal },
    { "D3DXVec4Transform",        (recomp_func_t)dx_D3DXVec4Transform },
    { "D3DXMatrixTranspose",      (recomp_func_t)dx_D3DXMatrixTranspose },
    { "D3DXMatrixMultiplyTranspose", (recomp_func_t)dx_D3DXMatrixMultiplyTranspose },
    { "D3DXPlaneTransform",       (recomp_func_t)dx_D3DXPlaneTransform },
    { "D3DXPlaneNormalize",       (recomp_func_t)dx_D3DXPlaneNormalize },
    { "D3DXAssembleShader",             (recomp_func_t)dx_D3DXAssembleShader },
    { "D3DXCompileShader",              (recomp_func_t)dx_D3DXCompileShader },
    { "D3DXCreateBuffer",               (recomp_func_t)dx_D3DXCreateBuffer },
    { "D3DXCreateTextureFromFileInMemory", (recomp_func_t)dx_D3DXCreateTextureFromFileInMemory },
    { "D3DXMatrixInverse",              (recomp_func_t)dx_D3DXMatrixInverse },
    { "D3DXMatrixMultiply",             (recomp_func_t)dx_D3DXMatrixMultiply },
    { "D3DXMatrixRotationYawPitchRoll", (recomp_func_t)dx_D3DXMatrixRotationYawPitchRoll },
    { "D3DXVec3Normalize",              (recomp_func_t)dx_D3DXVec3Normalize },
    { "D3DXLoadSurfaceFromSurface",     (recomp_func_t)dx_D3DXLoadSurfaceFromSurface },
    { "D3DXSaveTextureToFileA",         (recomp_func_t)dx_D3DXSaveTextureToFileA },
    { "D3DXAssembleShaderFromFileA",    (recomp_func_t)dx_D3DXAssembleShaderFromFileA },
    { "D3DXCompileShaderFromFileA",     (recomp_func_t)dx_D3DXCompileShaderFromFileA },
    { "D3DXGetShaderConstantTable",     (recomp_func_t)dx_D3DXGetShaderConstantTable },
    { "D3DXGetImageInfoFromFileInMemory", (recomp_func_t)dx_D3DXGetImageInfoFromFileInMemory },
    { "D3DXCreateCubeTextureFromFileInMemory", (recomp_func_t)dx_D3DXCreateCubeTextureFromFileInMemory },
    { "D3DXCreateVolumeTextureFromFileInMemory", (recomp_func_t)dx_D3DXCreateVolumeTextureFromFileInMemory },
    { "D3DXGetVertexShaderProfile",     (recomp_func_t)dx_D3DXGetVertexShaderProfile },
    { "D3DXGetPixelShaderProfile",      (recomp_func_t)dx_D3DXGetPixelShaderProfile },

    /* USER32 — caminho do WinMain (faltavam: abortavam o boot) */
    { "GetWindow",               (recomp_func_t)u_GetWindow },
    { "ShowCursor",              (recomp_func_t)u_ShowCursor },
    { "GetDoubleClickTime",      (recomp_func_t)u_GetDoubleClickTime },
    { "SwapMouseButton",         (recomp_func_t)u_SwapMouseButton },
    { "UnhookWindowsHookEx",     (recomp_func_t)u_UnhookWindowsHookEx },
    { "SetWindowsHookExA",       (recomp_func_t)u_SetWindowsHookExA },
    { "CallNextHookEx",          (recomp_func_t)u_CallNextHookEx },
    { "EnumChildWindows",        (recomp_func_t)u_EnumChildWindows },
    { "GetActiveWindow",         (recomp_func_t)u_GetActiveWindow },
    { "GetAsyncKeyState",        (recomp_func_t)u_GetAsyncKeyState },
    { "GetClassLongA",           (recomp_func_t)u_GetClassLongA },
    { "EnumDisplayDevicesA",     (recomp_func_t)u_EnumDisplayDevicesA },
    { "AdjustWindowRect",        (recomp_func_t)u_AdjustWindowRect },
    { "SendInput",               (recomp_func_t)u_SendInput },
    { "GetWindowTextA",          (recomp_func_t)u_GetWindowTextA },

    /* KERNEL32 — tempo/console/debug */
    { "CompareFileTime",         (recomp_func_t)k2_CompareFileTime },
    { "FormatMessageA",          (recomp_func_t)k2_FormatMessageA },
    { "FileTimeToSystemTime",    (recomp_func_t)k2_FileTimeToSystemTime },
    { "FileTimeToLocalFileTime", (recomp_func_t)k2_FileTimeToLocalFileTime },
    { "SystemTimeToTzSpecificLocalTime", (recomp_func_t)k2_SystemTimeToTzSpecificLocalTime },
    { "DebugBreak",              (recomp_func_t)k2_DebugBreak },
    { "SetFilePointerEx",        (recomp_func_t)k2_SetFilePointerEx },
    { "WriteConsoleA",           (recomp_func_t)k2_WriteConsoleA },
    { "WriteConsoleW",           (recomp_func_t)k2_WriteConsoleW },

    /* binkw32 — vídeos de logo (stdcall decorado @N) */
    { "_BinkOpen@8",             (recomp_func_t)b_BinkOpen },
    { "_BinkOpenDirectSound@4",  (recomp_func_t)b_BinkOpenDirectSound },
    { "_BinkClose@4",            (recomp_func_t)b_BinkClose },
    { "_BinkDoFrame@4",          (recomp_func_t)b_BinkDoFrame },
    { "_BinkNextFrame@4",        (recomp_func_t)b_BinkNextFrame },
    { "_BinkWait@4",             (recomp_func_t)b_BinkWait },
    { "_BinkPause@8",            (recomp_func_t)b_BinkPause },
    { "_BinkSetSoundSystem@8",   (recomp_func_t)b_BinkSetSoundSystem },
    { "_BinkCopyToBufferRect@44",(recomp_func_t)b_BinkCopyToBufferRect },

    /* d3d9.dll + faltantes de OLE/steam */
    { "D3DPERF_SetOptions",      (recomp_func_t)p_D3DPERF_SetOptions },
    { "CoFreeUnusedLibraries",   (recomp_func_t)o_CoFreeUnusedLibraries },
    { "SteamAPI_UnregisterCallback", (recomp_func_t)s_SteamAPI_UnregisterCallback },
    { NULL, NULL },
};

void dd_stubs_boot_register(void) {
    win32hle_register(stubs_boot);
    fprintf(stderr, "[DesertDroid] %zu stubs de boot registrados\n",
            sizeof(stubs_boot) / sizeof(stubs_boot[0]) - 1);
}
