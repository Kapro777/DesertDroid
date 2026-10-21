/* DesertDroid — Direct3D 9 HLE (M4: bring-up de video).
 *
 * O FNV carrega d3d9.dll dinamicamente: GetModuleHandleW("d3d9.dll") +
 * GetProcAddress("Direct3DCreate9"). Respondemos com um objeto COM
 * visivel ao guest (arena) cuja vtable aponta para shims auto-anunciantes.
 * Cada chamada do jogo nomeia o proximo metodo a implementar de verdade
 * (a ponte para DXVK-Native vem depois; primeiro o mapa de chamadas).
 *
 * COM x86 usa THISCALL: "this" vem em ECX (g_ecx no nosso modelo).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "win32hle.h"
#include "guest_mem.h"

extern uint32_t g_ecx;
extern uint32_t g_cur_func;

static void dd_trace(const char *what)
{
    fprintf(stderr, "[d3d9] %s (this=%#x, guest sub_%08X)\n",
            what, g_ecx, g_cur_func);
}

/* ---- helper de objeto COM guest ---- */
static uint32_t com_object(const char *const *methods, int n)
{
    uint32_t vtable = guest_alloc(4u * (uint32_t)n);
    uint32_t *vt = (uint32_t *)guest_ptr(vtable);
    for (int i = 0; i < n; i++) {
        vt[i] = hle_resolve(methods[i]);
        if (!vt[i]) {
            fprintf(stderr, "[d3d9] shim nao registrado: %s\n", methods[i]);
            return 0;
        }
    }
    uint32_t obj = guest_alloc(4);
    *(uint32_t *)guest_ptr(obj) = vtable;
    return obj;
}

/* ---- IDirect3D9 (vtable em ordem oficial do SDK) ----
 * Indices: 0 QueryInterface, 1 AddRef, 2 Release, 3 RegisterSoftwareDevice,
 * 4 GetAdapterCount, 5 GetAdapterIdentifier, 6 GetAdapterModeCount,
 * 7 EnumAdapterModes, 8 GetAdapterDisplayMode, 9 CheckDeviceType,
 * 10 CheckDeviceFormat, 11 CheckDeviceMultiSampleType,
 * 12 CheckDepthStencilMatch, 13 CheckDeviceFormatConversion,
 * 14 GetDeviceCaps, 15 GetAdapterMonitor, 16 CreateDevice */
static void m3d9_QueryInterface(void) { dd_trace("QueryInterface");
    *(uint32_t *)APTR(1) = *(uint32_t *)guest_ptr(g_ecx); RET(0, 2); }
static void m3d9_AddRef(void)  { dd_trace("AddRef");  RET(1, 0); }
static void m3d9_Release(void) { dd_trace("Release"); RET(1, 0); }
static void m3d9_RegisterSoftwareDevice(void) { dd_trace("RegisterSoftwareDevice"); RET(0, 1); }

static void m3d9_GetAdapterCount(void) { dd_trace("GetAdapterCount"); RET(1, 0); }

static void m3d9_GetAdapterIdentifier(void)
{
    dd_trace("GetAdapterIdentifier");
    /* D3DADAPTER_IDENTIFIER9 ~ 960 bytes; zerar basta pro boot */
    memset((void *)APTR(2), 0, 960);
    RET(0, 3);   /* D3D_OK */
}

static void m3d9_GetAdapterModeCount(void) { dd_trace("GetAdapterModeCount");
    RET(1, 2); } /* 1 modo */

/* EnumAdapterModes(adapter, format, index, pMode) */
static void m3d9_EnumAdapterModes(void)
{
    dd_trace("EnumAdapterModes");
    uint32_t *mode = (uint32_t *)APTR(3);
    if (mode) { mode[0] = 1280; mode[1] = 720; mode[2] = 60; mode[3] = 1; }
    RET(0, 4);   /* D3D_OK — 1280x720@60, D3DFMT_A8R8G8B8 */
}

static void m3d9_GetAdapterDisplayMode(void)
{
    dd_trace("GetAdapterDisplayMode");
    uint32_t *mode = (uint32_t *)APTR(1);
    if (mode) { mode[0] = 1280; mode[1] = 720; mode[2] = 60; mode[3] = 21; }
    RET(0, 2);
}

/* checagens: tudo suportado, D3D_OK */
static void m3d9_CheckDeviceType(void)          { dd_trace("CheckDeviceType");          RET(0, 5); }
static void m3d9_CheckDeviceFormat(void)         { dd_trace("CheckDeviceFormat");        RET(0, 6); }
static void m3d9_CheckDeviceMultiSampleType(void){ dd_trace("CheckDeviceMultiSampleType"); RET(0, 6); }
static void m3d9_CheckDepthStencilMatch(void)   { dd_trace("CheckDepthStencilMatch");   RET(0, 5); }
static void m3d9_CheckDeviceFormatConversion(void){ dd_trace("CheckDeviceFormatConversion"); RET(0, 5); }

static void m3d9_GetDeviceCaps(void)
{
    dd_trace("GetDeviceCaps");
    memset((void *)APTR(3), 0, 2048);   /* D3DCAPS9 zerado */
    RET(0, 3);
}

static void m3d9_GetAdapterMonitor(void) { dd_trace("GetAdapterMonitor"); RET(0x10004, 1); }

/* CreateDevice(adapter, devtype, hFocusWindow, behavior, pparams, ppDevice)
 * Devolve um device COM com vtable auto-anunciante. */
static const char *const k_device_methods[];

static void m3d9_CreateDevice(void)
{
    dd_trace("CreateDevice");
    uint32_t *out = (uint32_t *)APTR(5);
    if (!out) RET(0x88760866u, 6);          /* D3DERR_INVALIDCALL */
    int n = 0; while (k_device_methods[n]) n++;
    uint32_t dev = com_object(k_device_methods, n);
    if (!dev) RET(0x88760866u, 6);
    *out = dev;
    fprintf(stderr, "[d3d9] device entregue ao guest em %#x\n", dev);
    RET(0, 6);                              /* D3D_OK */
}

static const char *const k_d3d9_methods[] = {
    "D3D9_IDirect3D9_QueryInterface", "D3D9_IDirect3D9_AddRef",
    "D3D9_IDirect3D9_Release", "D3D9_IDirect3D9_RegisterSoftwareDevice",
    "D3D9_IDirect3D9_GetAdapterCount", "D3D9_IDirect3D9_GetAdapterIdentifier",
    "D3D9_IDirect3D9_GetAdapterModeCount", "D3D9_IDirect3D9_EnumAdapterModes",
    "D3D9_IDirect3D9_GetAdapterDisplayMode", "D3D9_IDirect3D9_CheckDeviceType",
    "D3D9_IDirect3D9_CheckDeviceFormat", "D3D9_IDirect3D9_CheckDeviceMultiSampleType",
    "D3D9_IDirect3D9_CheckDepthStencilMatch", "D3D9_IDirect3D9_CheckDeviceFormatConversion",
    "D3D9_IDirect3D9_GetDeviceCaps", "D3D9_IDirect3D9_GetAdapterMonitor",
    "D3D9_IDirect3D9_CreateDevice",
    NULL
};

/* ---- Direct3DCreate9(SDKVersion) -> IDirect3D9* ---- */
static void k_Direct3DCreate9(void)
{
    fprintf(stderr, "[d3d9] Direct3DCreate9(SDK=%#x) — fornecendo COM HLE\n", A32(0));
    int n = 0; while (k_d3d9_methods[n]) n++;
    uint32_t obj = com_object(k_d3d9_methods, n);
    RET(obj, 1);   /* NULL (0) seria "sem Direct3D" */
}

/* ---- metodos do device: todos auto-anunciantes ---- */
#define DEVM(name, argc, ret) \
    static void name(void) { dd_trace(#name); RET((ret), (argc)); }

/* Os que o FNV chama no arranjo grafico — valores benignos para andar o maximo */
DEVM(d_QueryInterface,  2, 0)
DEVM(d_AddRef,         0, 1)
DEVM(d_Release,         0, 1)
DEVM(d_TestCooperativeLevel, 0, 0)          /* D3D_OK */
DEVM(d_GetAvailableTextureMem, 0, 256u << 20)
DEVM(d_EvictManagedResources, 0, 0)
DEVM(d_GetDirect3D,    1, 0)
DEVM(d_GetDeviceCaps,  1, 0)
DEVM(d_GetDisplayMode, 2, 0)
DEVM(d_SetDisplayMode, 3, 0)
DEVM(d_GetCreationParameters, 1, 0)
DEVM(d_SetCursorProperties, 4, 0)
DEVM(d_SetCursorPosition, 3, 0)
DEVM(d_ShowCursor,     1, 1)
DEVM(d_CreateAdditionalSwapChain, 2, 0x88760866u)
DEVM(d_GetSwapChain,   3, 0x88760866u)
DEVM(d_GetNumberOfSwapChains, 0, 0)
DEVM(d_Reset,          1, 0)
DEVM(d_Present,        3, 0)
DEVM(d_GetBackBuffer,  4, 0x88760866u)
DEVM(d_GetRasterStatus, 2, 0)
DEVM(d_SetDialogBoxMode, 1, 0x88760866u)
DEVM(d_SetGammaRamp,   3, 0)
DEVM(d_GetGammaRamp,   3, 0)
DEVM(d_BeginScene,     0, 0)
DEVM(d_EndScene,       0, 0)
DEVM(d_Clear,          6, 0)
DEVM(d_SetTransform,   2, 0)
DEVM(d_GetTransform,   2, 0)
DEVM(d_SetRenderState, 2, 0)
DEVM(d_GetRenderState, 2, 0)
DEVM(d_SetTexture,     2, 0)
DEVM(d_SetFVF,         1, 0)
DEVM(d_SetPixelShader, 1, 0)
DEVM(d_SetVertexShader, 1, 0)
DEVM(d_DrawIndexedPrimitive, 7, 0)
DEVM(d_DrawPrimitive,  5, 0)
DEVM(d_CreateTexture,  7, 0x88760866u)
DEVM(d_CreateIndexBuffer, 6, 0x88760866u)
DEVM(d_CreateVertexBuffer, 6, 0x88760866u)
DEVM(d_CreatePixelShader,  2, 0x88760866u)
DEVM(d_CreateVertexShader, 2, 0x88760866u)

static const win32hle_shim dd_shims_d3d9_reg[] = {
    { "Direct3DCreate9", k_Direct3DCreate9 },
    { "D3D9_IDirect3D9_QueryInterface", m3d9_QueryInterface },
    { "D3D9_IDirect3D9_AddRef", m3d9_AddRef },
    { "D3D9_IDirect3D9_Release", m3d9_Release },
    { "D3D9_IDirect3D9_RegisterSoftwareDevice", m3d9_RegisterSoftwareDevice },
    { "D3D9_IDirect3D9_GetAdapterCount", m3d9_GetAdapterCount },
    { "D3D9_IDirect3D9_GetAdapterIdentifier", m3d9_GetAdapterIdentifier },
    { "D3D9_IDirect3D9_GetAdapterModeCount", m3d9_GetAdapterModeCount },
    { "D3D9_IDirect3D9_EnumAdapterModes", m3d9_EnumAdapterModes },
    { "D3D9_IDirect3D9_GetAdapterDisplayMode", m3d9_GetAdapterDisplayMode },
    { "D3D9_IDirect3D9_CheckDeviceType", m3d9_CheckDeviceType },
    { "D3D9_IDirect3D9_CheckDeviceFormat", m3d9_CheckDeviceFormat },
    { "D3D9_IDirect3D9_CheckDeviceMultiSampleType", m3d9_CheckDeviceMultiSampleType },
    { "D3D9_IDirect3D9_CheckDepthStencilMatch", m3d9_CheckDepthStencilMatch },
    { "D3D9_IDirect3D9_CheckDeviceFormatConversion", m3d9_CheckDeviceFormatConversion },
    { "D3D9_IDirect3D9_GetDeviceCaps", m3d9_GetDeviceCaps },
    { "D3D9_IDirect3D9_GetAdapterMonitor", m3d9_GetAdapterMonitor },
    { "D3D9_IDirect3D9_CreateDevice", m3d9_CreateDevice },
    { "D3D9_Device_QueryInterface", d_QueryInterface },
    { "D3D9_Device_AddRef", d_AddRef },
    { "D3D9_Device_Release", d_Release },
    { "D3D9_Device_TestCooperativeLevel", d_TestCooperativeLevel },
    { "D3D9_Device_GetAvailableTextureMem", d_GetAvailableTextureMem },
    { "D3D9_Device_EvictManagedResources", d_EvictManagedResources },
    { "D3D9_Device_GetDirect3D", d_GetDirect3D },
    { "D3D9_Device_GetDeviceCaps", d_GetDeviceCaps },
    { "D3D9_Device_GetDisplayMode", d_GetDisplayMode },
    { "D3D9_Device_SetDisplayMode", d_SetDisplayMode },
    { "D3D9_Device_GetCreationParameters", d_GetCreationParameters },
    { "D3D9_Device_SetCursorProperties", d_SetCursorProperties },
    { "D3D9_Device_SetCursorPosition", d_SetCursorPosition },
    { "D3D9_Device_ShowCursor", d_ShowCursor },
    { "D3D9_Device_CreateAdditionalSwapChain", d_CreateAdditionalSwapChain },
    { "D3D9_Device_GetSwapChain", d_GetSwapChain },
    { "D3D9_Device_GetNumberOfSwapChains", d_GetNumberOfSwapChains },
    { "D3D9_Device_Reset", d_Reset },
    { "D3D9_Device_Present", d_Present },
    { "D3D9_Device_GetBackBuffer", d_GetBackBuffer },
    { "D3D9_Device_GetRasterStatus", d_GetRasterStatus },
    { "D3D9_Device_SetDialogBoxMode", d_SetDialogBoxMode },
    { "D3D9_Device_SetGammaRamp", d_SetGammaRamp },
    { "D3D9_Device_GetGammaRamp", d_GetGammaRamp },
    { "D3D9_Device_BeginScene", d_BeginScene },
    { "D3D9_Device_EndScene", d_EndScene },
    { "D3D9_Device_Clear", d_Clear },
    { "D3D9_Device_SetTransform", d_SetTransform },
    { "D3D9_Device_GetTransform", d_GetTransform },
    { "D3D9_Device_SetRenderState", d_SetRenderState },
    { "D3D9_Device_GetRenderState", d_GetRenderState },
    { "D3D9_Device_SetTexture", d_SetTexture },
    { "D3D9_Device_SetFVF", d_SetFVF },
    { "D3D9_Device_SetPixelShader", d_SetPixelShader },
    { "D3D9_Device_SetVertexShader", d_SetVertexShader },
    { "D3D9_Device_DrawIndexedPrimitive", d_DrawIndexedPrimitive },
    { "D3D9_Device_DrawPrimitive", d_DrawPrimitive },
    { "D3D9_Device_CreateTexture", d_CreateTexture },
    { "D3D9_Device_CreateIndexBuffer", d_CreateIndexBuffer },
    { "D3D9_Device_CreateVertexBuffer", d_CreateVertexBuffer },
    { "D3D9_Device_CreatePixelShader", d_CreatePixelShader },
    { "D3D9_Device_CreateVertexShader", d_CreateVertexShader },
    { NULL, NULL }
};

/* vtable do device, em ordem IDirect3DDevice9 (subset auto-anunciante) */
static const char *const k_device_methods[] = {
    "D3D9_Device_QueryInterface", "D3D9_Device_AddRef", "D3D9_Device_Release",
    "D3D9_Device_TestCooperativeLevel", "D3D9_Device_GetAvailableTextureMem",
    "D3D9_Device_EvictManagedResources", "D3D9_Device_GetDirect3D",
    "D3D9_Device_GetDeviceCaps", "D3D9_Device_GetDisplayMode",
    "D3D9_Device_SetDisplayMode", "D3D9_Device_GetCreationParameters",
    "D3D9_Device_SetCursorProperties", "D3D9_Device_SetCursorPosition",
    "D3D9_Device_ShowCursor", "D3D9_Device_CreateAdditionalSwapChain",
    "D3D9_Device_GetSwapChain", "D3D9_Device_GetNumberOfSwapChains",
    "D3D9_Device_Reset", "D3D9_Device_Present", "D3D9_Device_GetBackBuffer",
    "D3D9_Device_GetRasterStatus", "D3D9_Device_SetDialogBoxMode",
    "D3D9_Device_SetGammaRamp", "D3D9_Device_GetGammaRamp",
    "D3D9_Device_BeginScene", "D3D9_Device_EndScene", "D3D9_Device_Clear",
    "D3D9_Device_SetTransform", "D3D9_Device_GetTransform",
    "D3D9_Device_SetRenderState", "D3D9_Device_GetRenderState",
    "D3D9_Device_SetTexture", "D3D9_Device_SetFVF",
    "D3D9_Device_SetPixelShader", "D3D9_Device_SetVertexShader",
    "D3D9_Device_DrawIndexedPrimitive", "D3D9_Device_DrawPrimitive",
    "D3D9_Device_CreateTexture", "D3D9_Device_CreateIndexBuffer",
    "D3D9_Device_CreateVertexBuffer", "D3D9_Device_CreatePixelShader",
    "D3D9_Device_CreateVertexShader",
    NULL
};

void dd_d3d9_register(void)
{
    win32hle_register(dd_shims_d3d9_reg);
}

/* ---- GetProcAddress de verdade (override newest-wins) ---- */
static void k_GetProcAddress(void)
{
    /* (hModule, lpProcName) — resolve nomes que temos registrados */
    const char *name = (const char *)APTR(1);
    uint32_t va = name ? hle_resolve(name) : 0;
    fprintf(stderr, "[dynload] GetProcAddress(\"%s\") -> %#x\n",
            name ? name : "(null)", va);
    RET(va, 2);
}

static const win32hle_shim dd_shims_dynload[] = {
    { "GetProcAddress", k_GetProcAddress },
    { NULL, NULL }
};

void dd_dynload_register(void)
{
    win32hle_register(dd_shims_dynload);
}
