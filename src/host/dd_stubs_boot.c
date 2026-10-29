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

/* GetSystemMetrics(nIndex) -> int.
 * Return per-index values; a generic nonzero value makes Boolean metrics such
 * as SM_REMOTESESSION incorrectly report true on headless/non-Windows hosts. */
static void k_GetSystemMetrics(void) {
    enum {
        DD_SM_CXSCREEN = 0,
        DD_SM_CYSCREEN = 1,
        DD_SM_CMONITORS = 80,
        DD_SM_REMOTESESSION = 0x1000,
        DD_SM_REMOTECONTROL = 0x2001
    };
    extern uint32_t dd_mem32(uint32_t va);
    extern uint32_t g_eax;
    uint32_t index = dd_mem32(g_esp + 4u);
    uint32_t value;

    switch (index) {
    case DD_SM_CXSCREEN:       value = 1280u; break;
    case DD_SM_CYSCREEN:       value = 720u; break;
    case DD_SM_CMONITORS:      value = 1u; break;
    case DD_SM_REMOTESESSION:
    case DD_SM_REMOTECONTROL:  value = 0u; break;
    default:                   value = 0u; break;
    }

    if (index == DD_SM_REMOTESESSION)
        fprintf(stderr, "[GetSystemMetrics] SM_REMOTESESSION=0\\n");
    g_eax = value;
    RET(1, 1);
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

static void d_Direct3DCreate9(void) {
    extern uint32_t g_eax;
    extern uint32_t guest_alloc(size_t);
    extern void *guest_ptr(uint32_t);
    uint32_t obj = guest_alloc(256);
    memset(guest_ptr(obj), 0, 256);
    g_eax = obj;
    RET(1, 1);
}

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

/* ---------- ADVAPI32 ---------- */
static void a_RegCloseKey(void)         { RET(0, 1); }
static void a_RegCreateKeyExA(void)     { RET(0, 9); }
static void a_RegOpenKeyExA(void)       { RET(0, 5); }
static void a_RegQueryValueExA(void)    { RET(0, 6); }
static void a_RegSetValueExA(void)      { RET(0, 6); }

/* ---------- d3dx9_38 (stubs genéricos) ---------- */
static void dx_stub(void) { RET(0, 4); }

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

    /* D3D9 */
    { "Direct3DCreate9",          (recomp_func_t)d_Direct3DCreate9 },

    /* OLE32 */
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

    /* d3dx9_38 — stubs genéricos */
    { "D3DXAssembleShader",             (recomp_func_t)dx_stub },
    { "D3DXCompileShader",              (recomp_func_t)dx_stub },
    { "D3DXCreateBuffer",               (recomp_func_t)dx_stub },
    { "D3DXCreateTextureFromFileInMemory", (recomp_func_t)dx_stub },
    { "D3DXMatrixInverse",              (recomp_func_t)dx_stub },
    { "D3DXMatrixMultiply",             (recomp_func_t)dx_stub },
    { "D3DXMatrixRotationYawPitchRoll", (recomp_func_t)dx_stub },
    { "D3DXVec3Normalize",              (recomp_func_t)dx_stub },
    { "D3DXVec3TransformCoord",         (recomp_func_t)dx_stub },
    { NULL, NULL },
};

void dd_stubs_boot_register(void) {
    win32hle_register(stubs_boot);
    fprintf(stderr, "[DesertDroid] %zu stubs de boot registrados\n",
            sizeof(stubs_boot) / sizeof(stubs_boot[0]) - 1);
}
