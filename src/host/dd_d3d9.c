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

/* ===== Implementações reais dos métodos do device (M4) ===== */

/* Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion) -> HRESULT */
static void d_Present(void)
{
    dd_trace("Present");
    /* No boot, apenas devolve D3D_OK; o swapchain real vem com DXVK-Native */
    RET(0, 4);
}

/* Reset(pPresentationParameters) -> HRESULT */
static void d_Reset(void)
{
    dd_trace("Reset");
    /* Recebe D3DPRESENT_PARAMETERS* em APTR(1); no boot aceita sem fazer nada */
    RET(0, 2);
}

/* CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle) -> HRESULT */
static void d_CreateTexture(void)
{
    dd_trace("CreateTexture");
    uint32_t width = A32(0), height = A32(1), levels = A32(2), usage = A32(3),
             format = A32(4), pool = A32(5);
    uint32_t *ppTex = (uint32_t *)APTR(6);
    uint32_t *pShared = (uint32_t *)APTR(7);

    /* Cria textura guest (stub): aloca objeto COM com vtable mínima */
    if (!ppTex) RET(0x88760866u, 8);  /* D3DERR_INVALIDCALL */

    static const char *const tex_methods[] = {
        "D3D9_Texture_QueryInterface", "D3D9_Texture_AddRef", "D3D9_Texture_Release",
        "D3D9_Texture_GetDevice", "D3D9_Texture_SetPrivateData", "D3D9_Texture_GetPrivateData",
        "D3D9_Texture_FreePrivateData", "D3D9_Texture_SetPriority", "D3D9_Texture_GetPriority",
        "D3D9_Texture_PreLoad", "D3D9_Texture_GetType", "D3D9_Texture_GetLevelCount",
        "D3D9_Texture_GetLevelDesc", "D3D9_Texture_LockRect", "D3D9_Texture_UnlockRect",
        "D3D9_Texture_GetSurfaceLevel", NULL
    };

    int n = 0; while (tex_methods[n]) n++;
    uint32_t tex = com_object(tex_methods, n);
    if (!tex) RET(0x88760866u, 8);
    *ppTex = tex;
    if (pShared) *pShared = 0;
    fprintf(stderr, "[d3d9] CreateTexture %ux%u lvl=%u fmt=%#x -> %#x\n", width, height, levels, format, tex);
    RET(0, 8);
}

/* CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle) -> HRESULT */
static void d_CreateVertexBuffer(void)
{
    dd_trace("CreateVertexBuffer");
    uint32_t length = A32(0), usage = A32(1), fvf = A32(2), pool = A32(3);
    uint32_t *ppVB = (uint32_t *)APTR(4), *pShared = (uint32_t *)APTR(5);
    if (!ppVB) RET(0x88760866u, 6);

    static const char *const vb_methods[] = {
        "D3D9_VB_QueryInterface", "D3D9_VB_AddRef", "D3D9_VB_Release",
        "D3D9_VB_GetDevice", "D3D9_VB_SetPrivateData", "D3D9_VB_GetPrivateData",
        "D3D9_VB_FreePrivateData", "D3D9_VB_SetPriority", "D3D9_VB_GetPriority",
        "D3D9_VB_PreLoad", "D3D9_VB_GetType", "D3D9_VB_Lock", "D3D9_VB_Unlock",
        "D3D9_VB_GetDesc", NULL
    };

    int n = 0; while (vb_methods[n]) n++;
    uint32_t vb = com_object(vb_methods, n);
    if (!vb) RET(0x88760866u, 6);
    *ppVB = vb;
    if (pShared) *pShared = 0;
    fprintf(stderr, "[d3d9] CreateVertexBuffer len=%u usage=%#x fvf=%#x -> %#x\n", length, usage, fvf, vb);
    RET(0, 6);
}

/* CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle) -> HRESULT */
static void d_CreateIndexBuffer(void)
{
    dd_trace("CreateIndexBuffer");
    uint32_t length = A32(0), usage = A32(1), format = A32(2), pool = A32(3);
    uint32_t *ppIB = (uint32_t *)APTR(4), *pShared = (uint32_t *)APTR(5);
    if (!ppIB) RET(0x88760866u, 6);

    static const char *const ib_methods[] = {
        "D3D9_IB_QueryInterface", "D3D9_IB_AddRef", "D3D9_IB_Release",
        "D3D9_IB_GetDevice", "D3D9_IB_SetPrivateData", "D3D9_IB_GetPrivateData",
        "D3D9_IB_FreePrivateData", "D3D9_IB_SetPriority", "D3D9_IB_GetPriority",
        "D3D9_IB_PreLoad", "D3D9_IB_GetType", "D3D9_IB_Lock", "D3D9_IB_Unlock",
        "D3D9_IB_GetDesc", NULL
    };

    int n = 0; while (ib_methods[n]) n++;
    uint32_t ib = com_object(ib_methods, n);
    if (!ib) RET(0x88760866u, 6);
    *ppIB = ib;
    if (pShared) *pShared = 0;
    fprintf(stderr, "[d3d9] CreateIndexBuffer len=%u fmt=%#x -> %#x\n", length, format, ib);
    RET(0, 6);
}

/* SetTransform(State, pMatrix) -> HRESULT */
static void d_SetTransform(void)
{
    dd_trace("SetTransform");
    uint32_t state = A32(0);
    /* pMatrix em APTR(1) — matriz 4x4 float (64 bytes); no boot apenas ignora */
    RET(0, 2);
}

/* SetRenderState(State, Value) -> HRESULT */
static void d_SetRenderState(void)
{
    dd_trace("SetRenderState");
    uint32_t state = A32(0), value = A32(1);
    /* Guarda em estado interno se necessário; no boot apenas D3D_OK */
    RET(0, 2);
}

/* SetFVF(FVF) -> HRESULT */
static void d_SetFVF(void)
{
    dd_trace("SetFVF");
    RET(0, 1);
}

/* SetPixelShader(Shader) -> HRESULT */
static void d_SetPixelShader(void)
{
    dd_trace("SetPixelShader");
    RET(0, 1);
}

/* SetVertexShader(Shader) -> HRESULT */
static void d_SetVertexShader(void)
{
    dd_trace("SetVertexShader");
    RET(0, 1);
}

/* DrawIndexedPrimitive(Type, BaseIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount) -> HRESULT */
static void d_DrawIndexedPrimitive(void)
{
    dd_trace("DrawIndexedPrimitive");
    RET(0, 7);
}

/* DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount) -> HRESULT */
static void d_DrawPrimitive(void)
{
    dd_trace("DrawPrimitive");
    RET(0, 4);
}

/* Clear(Count, pRects, Flags, Color, Z, Stencil) -> HRESULT */
static void d_Clear(void)
{
    dd_trace("Clear");
    RET(0, 7);
}

/* BeginScene/EndScene -> HRESULT */
static void d_BeginScene(void)
{
    dd_trace("BeginScene");
    RET(0, 0);
}
static void d_EndScene(void)
{
    dd_trace("EndScene");
    RET(0, 0);
}

/* GetBackBuffer(SwapChain, Type, ppBackBuffer) -> HRESULT */
static void d_GetBackBuffer(void)
{
    dd_trace("GetBackBuffer");
    uint32_t *ppSurf = (uint32_t *)APTR(3);
    if (!ppSurf) RET(0x88760866u, 4);
    /* Surface stub */
    static const char *const surf_methods[] = {
        "D3D9_Surf_QueryInterface", "D3D9_Surf_AddRef", "D3D9_Surf_Release",
        "D3D9_Surf_GetDevice", "D3D9_Surf_SetPrivateData", "D3D9_Surf_GetPrivateData",
        "D3D9_Surf_FreePrivateData", "D3D9_Surf_SetPriority", "D3D9_Surf_GetPriority",
        "D3D9_Surf_PreLoad", "D3D9_Surf_GetType", "D3D9_Surf_GetContainer",
        "D3D9_Surf_GetDesc", "D3D9_Surf_LockRect", "D3D9_Surf_UnlockRect",
        "D3D9_Surf_GetDC", "D3D9_Surf_ReleaseDC", NULL
    };
    int n = 0; while (surf_methods[n]) n++;
    uint32_t surf = com_object(surf_methods, n);
    if (!surf) RET(0x88760866u, 4);
    *ppSurf = surf;
    RET(0, 4);
}

/* GetDisplayMode(SwapChain, pMode) -> HRESULT */
static void d_GetDisplayMode(void)
{
    dd_trace("GetDisplayMode");
    uint32_t *mode = (uint32_t *)APTR(1);
    if (mode) { mode[0] = 1280; mode[1] = 720; mode[2] = 60; mode[3] = 21; }
    RET(0, 2);
}

/* TestCooperativeLevel -> HRESULT */
static void d_TestCooperativeLevel(void)
{
    dd_trace("TestCooperativeLevel");
    RET(0, 0);  /* D3D_OK = device operational */
}

/* GetAvailableTextureMem -> DWORD */
static void d_GetAvailableTextureMem(void)
{
    dd_trace("GetAvailableTextureMem");
    RET(256u << 20, 0);  /* 256 MB */
}

/* EvictManagedResources -> HRESULT */
static void d_EvictManagedResources(void)
{
    dd_trace("EvictManagedResources");
    RET(0, 0);
}

/* GetDirect3D(ppD3D9) -> HRESULT */
static void d_GetDirect3D(void)
{
    dd_trace("GetDirect3D");
    uint32_t *pp = (uint32_t *)APTR(0);
    if (pp) *pp = g_ecx;  /* devolve o IDirect3D9 original (aprox) */
    RET(0, 1);
}

/* GetDeviceCaps(pCaps) -> HRESULT */
static void d_GetDeviceCaps(void)
{
    dd_trace("GetDeviceCaps");
    memset((void *)APTR(0), 0, 2048);
    RET(0, 1);
}

/* GetCreationParameters(pParams) -> HRESULT */
static void d_GetCreationParameters(void)
{
    dd_trace("GetCreationParameters");
    memset((void *)APTR(0), 0, 16);
    RET(0, 1);
}

/* SetTexture(Stage, pTexture) -> HRESULT */
static void d_SetTexture(void)
{
    dd_trace("SetTexture");
    RET(0, 2);
}

/* GetTransform/GetRenderState/GetGammaRamp — devolve defaults */
static void d_GetTransform(void)       { dd_trace("GetTransform");       memset((void *)APTR(1), 0, 64); RET(0, 2); }
static void d_GetRenderState(void)     { dd_trace("GetRenderState");     *(uint32_t *)APTR(1) = 0; RET(0, 2); }
static void d_GetGammaRamp(void)       { dd_trace("GetGammaRamp");       RET(0, 2); }

/* SetTransform/SetRenderState/SetGammaRamp — aceita e ignora */
static void d_SetGammaRamp(void)       { dd_trace("SetGammaRamp");       RET(0, 3); }

/* ===== Stubs APENAS para métodos SEM implementação manual ===== */
#define STUB(name, argc, ret) static void name(void) { dd_trace(#name); RET((ret), (argc)); }

/* Device methods que NÃO têm implementação manual dedicada */
STUB(d_QueryInterface,  2, 0)
STUB(d_AddRef,         0, 1)
STUB(d_Release,         0, 1)
STUB(d_ShowCursor,     1, 1)
STUB(d_CreateAdditionalSwapChain, 2, 0x88760866u)
STUB(d_GetSwapChain,   3, 0x88760866u)
STUB(d_GetNumberOfSwapChains, 0, 0)
STUB(d_SetDialogBoxMode, 1, 0x88760866u)
STUB(d_CreatePixelShader,  2, 0x88760866u)
STUB(d_CreateVertexShader, 2, 0x88760866u)
STUB(d_SetDisplayMode, 3, 0)
STUB(d_SetCursorProperties, 4, 0)
STUB(d_SetCursorPosition, 3, 0)
STUB(d_GetRasterStatus, 2, 0)

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

/* Métodos do device com implementação real abaixo (não usar DEVM para estes) */

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

/* ===== Stubs COM para texturas / vertex buffer / index buffer / surface ===== */

#define COM_STUB(name, argc, ret) \
    static void name(void) { dd_trace(#name); RET((ret), (argc)); }

/* Texture methods */
COM_STUB(D3D9_Texture_QueryInterface, 2, 0)
COM_STUB(D3D9_Texture_AddRef, 0, 1)
COM_STUB(D3D9_Texture_Release, 0, 1)
COM_STUB(D3D9_Texture_GetDevice, 1, 0)
COM_STUB(D3D9_Texture_SetPrivateData, 4, 0)
COM_STUB(D3D9_Texture_GetPrivateData, 4, 0)
COM_STUB(D3D9_Texture_FreePrivateData, 2, 0)
COM_STUB(D3D9_Texture_SetPriority, 1, 0)
COM_STUB(D3D9_Texture_GetPriority, 1, 1)
COM_STUB(D3D9_Texture_PreLoad, 0, 0)
COM_STUB(D3D9_Texture_GetType, 1, 3)   /* D3DRTYPE_TEXTURE */
COM_STUB(D3D9_Texture_GetLevelCount, 1, 1)
COM_STUB(D3D9_Texture_GetLevelDesc, 2, 0)
COM_STUB(D3D9_Texture_LockRect, 4, 0)
COM_STUB(D3D9_Texture_UnlockRect, 2, 0)
COM_STUB(D3D9_Texture_GetSurfaceLevel, 2, 0)

/* Vertex Buffer methods */
COM_STUB(D3D9_VB_QueryInterface, 2, 0)
COM_STUB(D3D9_VB_AddRef, 0, 1)
COM_STUB(D3D9_VB_Release, 0, 1)
COM_STUB(D3D9_VB_GetDevice, 1, 0)
COM_STUB(D3D9_VB_SetPrivateData, 4, 0)
COM_STUB(D3D9_VB_GetPrivateData, 4, 0)
COM_STUB(D3D9_VB_FreePrivateData, 2, 0)
COM_STUB(D3D9_VB_SetPriority, 1, 0)
COM_STUB(D3D9_VB_GetPriority, 1, 1)
COM_STUB(D3D9_VB_PreLoad, 0, 0)
COM_STUB(D3D9_VB_GetType, 1, 4)   /* D3DRTYPE_VERTEXBUFFER */
COM_STUB(D3D9_VB_Lock, 4, 0)
COM_STUB(D3D9_VB_Unlock, 1, 0)
COM_STUB(D3D9_VB_GetDesc, 2, 0)

/* Index Buffer methods */
COM_STUB(D3D9_IB_QueryInterface, 2, 0)
COM_STUB(D3D9_IB_AddRef, 0, 1)
COM_STUB(D3D9_IB_Release, 0, 1)
COM_STUB(D3D9_IB_GetDevice, 1, 0)
COM_STUB(D3D9_IB_SetPrivateData, 4, 0)
COM_STUB(D3D9_IB_GetPrivateData, 4, 0)
COM_STUB(D3D9_IB_FreePrivateData, 2, 0)
COM_STUB(D3D9_IB_SetPriority, 1, 0)
COM_STUB(D3D9_IB_GetPriority, 1, 1)
COM_STUB(D3D9_IB_PreLoad, 0, 0)
COM_STUB(D3D9_IB_GetType, 1, 5)   /* D3DRTYPE_INDEXBUFFER */
COM_STUB(D3D9_IB_Lock, 4, 0)
COM_STUB(D3D9_IB_Unlock, 1, 0)
COM_STUB(D3D9_IB_GetDesc, 2, 0)

/* Surface methods */
COM_STUB(D3D9_Surf_QueryInterface, 2, 0)
COM_STUB(D3D9_Surf_AddRef, 0, 1)
COM_STUB(D3D9_Surf_Release, 0, 1)
COM_STUB(D3D9_Surf_GetDevice, 1, 0)
COM_STUB(D3D9_Surf_SetPrivateData, 4, 0)
COM_STUB(D3D9_Surf_GetPrivateData, 4, 0)
COM_STUB(D3D9_Surf_FreePrivateData, 2, 0)
COM_STUB(D3D9_Surf_SetPriority, 1, 0)
COM_STUB(D3D9_Surf_GetPriority, 1, 1)
COM_STUB(D3D9_Surf_PreLoad, 0, 0)
COM_STUB(D3D9_Surf_GetType, 1, 1)   /* D3DRTYPE_SURFACE */
COM_STUB(D3D9_Surf_GetContainer, 2, 0)
COM_STUB(D3D9_Surf_GetDesc, 2, 0)
COM_STUB(D3D9_Surf_LockRect, 4, 0)
COM_STUB(D3D9_Surf_UnlockRect, 2, 0)
COM_STUB(D3D9_Surf_GetDC, 2, 0)
COM_STUB(D3D9_Surf_ReleaseDC, 1, 0)

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
