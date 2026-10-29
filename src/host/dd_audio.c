/* DesertDroid — Audio backend (DirectSound → AAudio stubs for M4).
 *
 * O FNV usa DirectSound (dsound.dll) para áudio. No Android, a API nativa
 * é AAudio (ou OpenSL ES legado). Este arquivo fornece shims HLE que o
 * win32hle chama quando o jogo importa DirectSoundCreate, etc.
 *
 * Implementação fase 1: stubs no-op que devolvem sucesso.
 * Fase 2 (futuro): bridge real para AAudio via JNI no APK.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "win32hle.h"

extern uint32_t g_eax;
extern uint32_t guest_alloc(size_t);
extern void *guest_ptr(uint32_t);

/* DirectSoundCreate(lpGuid, ppDS, pUnkOuter) -> HRESULT */
static void ds_DirectSoundCreate(void)
{
    /* lpGuid em APTR(0), ppDS em APTR(1), pUnkOuter em APTR(2) */
    uint32_t *ppDS = (uint32_t *)APTR(1);
    if (!ppDS) RET(0x80004003u, 3);  /* E_POINTER */

    /* Cria objeto IDirectSound stub na arena guest */
    static const char *const ds_methods[] = {
        "DS_QueryInterface", "DS_AddRef", "DS_Release",
        "DS_CreateSoundBuffer", "DS_GetCaps", "DS_DuplicateSoundBuffer",
        "DS_SetCooperativeLevel", "DS_Compact", "DS_GetSpeakerConfig",
        "DS_SetSpeakerConfig", "DS_Initialize", NULL
    };

    int n = 0; while (ds_methods[n]) n++;
    uint32_t obj = guest_alloc(8);  /* 4 bytes vtable ptr + 4 bytes refcount */
    uint32_t vtable = guest_alloc(4 * (n + 1));
    uint32_t *vt = (uint32_t *)guest_ptr(vtable);
    for (int i = 0; i < n; i++) {
        vt[i] = hle_resolve(ds_methods[i]);
        if (!vt[i]) vt[i] = 0;
    }
    vt[n] = 0;

    uint32_t *pobj = (uint32_t *)guest_ptr(obj);
    *pobj = vtable;
    pobj[1] = 1;  /* refcount = 1 */

    *ppDS = obj;
    fprintf(stderr, "[audio] DirectSoundCreate -> %#x\n", obj);
    RET(0, 3);  /* DS_OK */
}

/* DirectSoundEnumerateA/L — enumera dispositivos; no boot retorna callback ok */
static void ds_DirectSoundEnumerateA(void)
{
    fprintf(stderr, "[audio] DirectSoundEnumerateA\n");
    RET(0, 2);
}
static void ds_DirectSoundEnumerateW(void)
{
    fprintf(stderr, "[audio] DirectSoundEnumerateW\n");
    RET(0, 2);
}

/* DirectSoundCaptureCreate — captura (mic); no boot no-op */
static void ds_DirectSoundCaptureCreate(void)
{
    uint32_t *pp = (uint32_t *)APTR(1);
    if (!pp) RET(0x80004003u, 3);
    *pp = 0;
    RET(0, 3);
}

/* ===== IDirectSound stubs ===== */

#define DS_STUB(name, argc, ret) \
    static void name(void) { \
        fprintf(stderr, "[audio] %s\n", #name); \
        RET((ret), (argc)); \
    }

DS_STUB(DS_QueryInterface, 2, 0)
DS_STUB(DS_AddRef, 0, 1)
DS_STUB(DS_Release, 0, 1)
DS_STUB(DS_CreateSoundBuffer, 3, 0)
DS_STUB(DS_GetCaps, 1, 0)
DS_STUB(DS_DuplicateSoundBuffer, 3, 0)
DS_STUB(DS_SetCooperativeLevel, 2, 0)
DS_STUB(DS_Compact, 0, 0)
DS_STUB(DS_GetSpeakerConfig, 1, 0)
DS_STUB(DS_SetSpeakerConfig, 1, 0)
DS_STUB(DS_Initialize, 1, 0)

/* ===== IDirectSoundBuffer stubs ===== */
DS_STUB(DSB_QueryInterface, 2, 0)
DS_STUB(DSB_AddRef, 0, 1)
DS_STUB(DSB_Release, 0, 1)
DS_STUB(DSB_GetCaps, 1, 0)
DS_STUB(DSB_GetCurrentPosition, 2, 0)
DS_STUB(DSB_GetFormat, 1, 0)
DS_STUB(DSB_GetVolume, 1, 0)
DS_STUB(DSB_SetVolume, 2, 0)
DS_STUB(DSB_GetPan, 1, 0)
DS_STUB(DSB_SetPan, 2, 0)
DS_STUB(DSB_GetFrequency, 1, 0)
DS_STUB(DSB_SetFrequency, 2, 0)
DS_STUB(DSB_GetStatus, 1, 0)
DS_STUB(DSB_Initialize, 2, 0)
DS_STUB(DSB_Lock, 5, 0)
DS_STUB(DSB_Unlock, 3, 0)
DS_STUB(DSB_Play, 3, 0)
DS_STUB(DSB_Stop, 0, 0)
DS_STUB(DSB_SetCurrentPosition, 2, 0)
DS_STUB(DSB_SetFormat, 1, 0)
DS_STUB(DSB_Restore, 0, 0)

/* ===== Tabela de registro ===== */
static const win32hle_shim dd_shims_audio[] = {
    /* dsound.dll */
    { "DirectSoundCreate",          ds_DirectSoundCreate },
    { "DirectSoundEnumerateA",      ds_DirectSoundEnumerateA },
    { "DirectSoundEnumerateW",      ds_DirectSoundEnumerateW },
    { "DirectSoundCaptureCreate",   ds_DirectSoundCaptureCreate },

    /* IDirectSound */
    { "DS_QueryInterface",  DS_QueryInterface },
    { "DS_AddRef",          DS_AddRef },
    { "DS_Release",         DS_Release },
    { "DS_CreateSoundBuffer", DS_CreateSoundBuffer },
    { "DS_GetCaps",         DS_GetCaps },
    { "DS_DuplicateSoundBuffer", DS_DuplicateSoundBuffer },
    { "DS_SetCooperativeLevel", DS_SetCooperativeLevel },
    { "DS_Compact",         DS_Compact },
    { "DS_GetSpeakerConfig", DS_GetSpeakerConfig },
    { "DS_SetSpeakerConfig", DS_SetSpeakerConfig },
    { "DS_Initialize",      DS_Initialize },

    /* IDirectSoundBuffer */
    { "DSB_QueryInterface",  DSB_QueryInterface },
    { "DSB_AddRef",          DSB_AddRef },
    { "DSB_Release",         DSB_Release },
    { "DSB_GetCaps",         DSB_GetCaps },
    { "DSB_GetCurrentPosition", DSB_GetCurrentPosition },
    { "DSB_GetFormat",       DSB_GetFormat },
    { "DSB_GetVolume",       DSB_GetVolume },
    { "DSB_SetVolume",       DSB_SetVolume },
    { "DSB_GetPan",          DSB_GetPan },
    { "DSB_SetPan",          DSB_SetPan },
    { "DSB_GetFrequency",    DSB_GetFrequency },
    { "DSB_SetFrequency",    DSB_SetFrequency },
    { "DSB_GetStatus",       DSB_GetStatus },
    { "DSB_Initialize",      DSB_Initialize },
    { "DSB_Lock",            DSB_Lock },
    { "DSB_Unlock",          DSB_Unlock },
    { "DSB_Play",            DSB_Play },
    { "DSB_Stop",            DSB_Stop },
    { "DSB_SetCurrentPosition", DSB_SetCurrentPosition },
    { "DSB_SetFormat",       DSB_SetFormat },
    { "DSB_Restore",         DSB_Restore },

    { NULL, NULL }
};

void dd_audio_register(void)
{
    win32hle_register(dd_shims_audio);
    fprintf(stderr, "[audio] DirectSound stubs registrados\n");
}