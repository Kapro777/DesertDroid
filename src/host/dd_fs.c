/* DesertDroid — camada de filesystem (dd_fs).
 *
 * O jogo deriva os proprios caminhos de GetModuleFileNameA e dos folders
 * (SHGetFolderPathA). Devolvemos caminhos WINDOWS virtuais enraizados em
 * "C:\FalloutNV" e traduzimos cada caminho que volta pelas APIs de arquivo
 * para a pasta REAL do exe do usuario (BYOG). Assim o jogo acha Data\,
 * cria seu INI de primeira execucao e enumera os .bsa.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include "win32hle.h"

#define DD_INVALID_HANDLE 0xFFFFFFFFu
#define VIRTUAL_ROOT "C:\\FalloutNV"

static char g_root[1024];        /* pasta real do jogo (host, '/') */
static int  g_root_len;

void dd_fs_set_root(const char *host_dir)
{
    snprintf(g_root, sizeof g_root, "%s", host_dir);
    g_root_len = (int)strlen(g_root);
    while (g_root_len > 1 && g_root[g_root_len - 1] == '/')
        g_root[--g_root_len] = 0;
    fprintf(stderr, "[DesertDroid] raiz de arquivos: %s (virtual: %s)\n",
            g_root, VIRTUAL_ROOT);
}

/* caminho Windows (do guest) -> caminho host. Regra: strip "X:\",
 * strip componente "FalloutNV" inicial (o da raiz virtual), junta. */
static const char *win_to_host(const char *winpath)
{
    static char out[2048];
    if (!winpath || !*winpath) return NULL;
    const char *p = winpath;
    if (p[0] && p[1] == ':' && (p[2] == '\\' || p[2] == '/'))
        p += 3;
    if (strncmp(p, "FalloutNV\\", 10) == 0 || strncmp(p, "FalloutNV/", 10) == 0)
        p += 10;
    if (strncmp(p, "FalloutNV", 9) == 0 && (p[9] == 0))
        p += 9;
    snprintf(out, sizeof out, "%s/%s", g_root, p);
    for (char *q = out + g_root_len + 1; *q; q++)
        if (*q == '\\') *q = '/';
    /* segura: resolve '..' besta subindo um nivel no maximo */
    if (strstr(out, "..")) return NULL;
    return out;
}

/* ---- GetModuleFileNameA(hModule, lpFilename, nSize) ---- */
static void k_GetModuleFileNameA(void)
{
    char *buf = (char *)APTR(1);
    uint32_t nsize = A32(2);
    if (!buf || nsize < sizeof VIRTUAL_ROOT) RET(0, 3);
    snprintf(buf, nsize, "%s\\FalloutNV.exe", VIRTUAL_ROOT);
    RET((uint32_t)strlen(buf) + 1, 3);
}

/* ---- FindFirstFileA(lpFileName, lpFindFileData) ---- */
#define MAX_OPENDIRS 32
typedef struct { DIR *d; char pattern[280]; int in_use; } findstate;
static findstate g_find[MAX_OPENDIRS];

typedef struct {
    uint32_t dwFileAttributes;
    uint32_t ftCreationTime[2], ftLastAccessTime[2], ftLastWriteTime[2];
    uint32_t nFileSizeHigh, nFileSizeLow;
    uint32_t dwReserved0, dwReserved1;
    char     cFileName[260];
    char     cAlternateFileName[14];
} WIN32_FIND_DATAA_G;

static void fill_find_data(const char *dirpath, const char *name,
                           WIN32_FIND_DATAA_G *fd)
{
    char full[2048];
    struct stat st;
    memset(fd, 0, sizeof *fd);
    snprintf(full, sizeof full, "%s/%s", dirpath, name);
    if (stat(full, &st) == 0) {
        if (S_ISDIR(st.st_mode))  fd->dwFileAttributes = 0x10;   /* DIRECTORY */
        else                     fd->dwFileAttributes = 0x80;   /* NORMAL    */
        uint64_t ft = ((uint64_t)st.st_mtime * 10000000ull) + 116444736000000000ull;
        fd->ftLastWriteTime[0] = (uint32_t)ft;
        fd->ftLastWriteTime[1] = (uint32_t)(ft >> 32);
        fd->ftCreationTime[0]  = fd->ftLastWriteTime[0];
        fd->ftCreationTime[1]  = fd->ftLastWriteTime[1];
        fd->nFileSizeLow = (uint32_t)st.st_size;
    } else {
        fd->dwFileAttributes = 0x80;
    }
    snprintf(fd->cFileName, sizeof fd->cFileName, "%s", name);
}

static void k_FindFirstFileA(void)
{
    const char *winpat = (const char *)APTR(0);
    WIN32_FIND_DATAA_G *fd = (WIN32_FIND_DATAA_G *)APTR(1);
    if (!winpat || !fd) RET(DD_INVALID_HANDLE, 2);

    /* separa diretorio e padrao ("Data\*.bsa" ou "*.bsa") */
    char full[2048];
    snprintf(full, sizeof full, "%s", winpat);
    for (char *q = full; *q; q++) if (*q == '\\') *q = '/';
    char *slash = strrchr(full, '/');
    char pattern[280];
    if (slash) { snprintf(pattern, sizeof pattern, "%s", slash + 1); *slash = 0; }
    else       { snprintf(pattern, sizeof pattern, "%s", full);       snprintf(full, sizeof full, "."); }

    const char *host = win_to_host(full);
    if (!host) RET(DD_INVALID_HANDLE, 2);
    DIR *d = opendir(host);
    if (!d) RET(DD_INVALID_HANDLE, 2);
    int slot = -1;
    for (int i = 0; i < MAX_OPENDIRS; i++) if (!g_find[i].in_use) { slot = i; break; }
    if (slot < 0) { closedir(d); RET(DD_INVALID_HANDLE, 2); }
    g_find[slot].in_use = 1;
    g_find[slot].d = d;
    snprintf(g_find[slot].pattern, sizeof g_find[slot].pattern, "%s", pattern);

    /* primeira entrada que casa */
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!fnmatch(g_find[slot].pattern, e->d_name, FNM_PATHNAME))
        /* nota: "*.bsa" casa via fnmatch; nomes com '.' nao sao ocultos aqui */
        {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) {
                if (strcmp(g_find[slot].pattern, "*") &&
                    strcmp(g_find[slot].pattern, "*.*") &&
                    strcmp(g_find[slot].pattern, "*.*") != 0) continue;
            }
            fill_find_data(host, e->d_name, fd);
            RET((uint32_t)(0x1000 + slot), 2);
        }
    }
    closedir(d);
    g_find[slot].in_use = 0;
    RET(DD_INVALID_HANDLE, 2);
}

static void k_FindNextFileA(void)
{
    uint32_t h = A32(0);
    WIN32_FIND_DATAA_G *fd = (WIN32_FIND_DATAA_G *)APTR(1);
    if (h < 0x1000 || h >= 0x1000 + MAX_OPENDIRS || !g_find[h - 0x1000].in_use)
        RET(0, 2);
    findstate *fs = &g_find[h - 0x1000];
    struct dirent *e;
    while ((e = readdir(fs->d))) {
        if (fnmatch(fs->pattern, e->d_name, FNM_PATHNAME) == 0 &&
            strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
            /* recupera o diretorio aberto para o fill */
            char dirpath[2048];
            int dfd = dirfd(fs->d);
            snprintf(dirpath, sizeof dirpath, "/proc/self/fd/%d", dfd);
            fill_find_data(dirpath, e->d_name, fd);
            RET(1, 2);
        }
    }
    RET(0, 2);   /* ERROR_NO_MORE_FILES */
}

static void k_FindClose(void)
{
    uint32_t h = A32(0);
    if (h >= 0x1000 && h < 0x1000 + MAX_OPENDIRS && g_find[h - 0x1000].in_use) {
        closedir(g_find[h - 0x1000].d);
        g_find[h - 0x1000].in_use = 0;
    }
    RET(1, 1);
}

/* ---- CreateDirectoryA(lpPathName, lpSecurityAttributes) — recursivo ---- */
static void k_CreateDirectoryA(void)
{
    const char *host = win_to_host((const char *)APTR(0));
    if (!host) RET(0, 2);
    char tmp[2048];
    snprintf(tmp, sizeof tmp, "%s", host);
    for (char *q = tmp + g_root_len + 1; *q; q++) {
        if (*q == '/') {
            *q = 0;
            mkdir(tmp, 0755);
            *q = '/';
        }
    }
    RET(mkdir(tmp, 0755) == 0 || errno == EEXIST ? 1u : 0u, 2);
}

/* ---- GetFileAttributesA(lpFileName) ---- */
static void k_GetFileAttributesA(void)
{
    const char *host = win_to_host((const char *)APTR(0));
    struct stat st;
    if (!host || stat(host, &st) != 0) RET(DD_INVALID_HANDLE, 1);
    RET(S_ISDIR(st.st_mode) ? 0x10u : 0x80u, 1);
}

/* ---- DeleteFileA / MoveFileA / CopyFileA ---- */
static void k_DeleteFileA(void)
{
    const char *host = win_to_host((const char *)APTR(0));
    RET(host && unlink(host) == 0 ? 1u : 0u, 1);
}
static void k_MoveFileA(void)
{
    const char *a = win_to_host((const char *)APTR(0));
    const char *b = win_to_host((const char *)APTR(1));
    RET(a && b && rename(a, b) == 0 ? 1u : 0u, 2);
}
static void k_CopyFileA(void)
{
    const char *a = win_to_host((const char *)APTR(0));
    const char *b = win_to_host((const char *)APTR(1));
    int ok = 0;
    if (a && b) {
        FILE *in = fopen(a, "rb"), *out = fopen(b, "wb");
        if (in && out) {
            char buf[65536]; size_t n;
            while ((n = fread(buf, 1, sizeof buf, in))) fwrite(buf, 1, n, out);
            ok = 1;
        }
        if (in) fclose(in);
        if (out) fclose(out);
    }
    RET(ok ? 1u : 0u, 3);
}

/* ---- ExitProcess com diagnostico (override de exit silencioso) ---- */
extern uint32_t g_cur_func;
extern const char *g_cur_import;
static void k_ExitProcess(void)
{
    fprintf(stderr, "[DesertDroid] ExitProcess(%u) chamado do guest "
            "sub_%08X (import: %s)\n",
            A32(0), g_cur_func, g_cur_import ? g_cur_import : "-");
    fflush(stderr);
    _exit((int)A32(0));
}

static const win32hle_shim dd_shims_fs[] = {
    { "GetModuleFileNameA", k_GetModuleFileNameA },
    { "FindFirstFileA",     k_FindFirstFileA },
    { "FindNextFileA",      k_FindNextFileA },
    { "FindClose",          k_FindClose },
    { "CreateDirectoryA",   k_CreateDirectoryA },
    { "GetFileAttributesA", k_GetFileAttributesA },
    { "DeleteFileA",        k_DeleteFileA },
    { "MoveFileA",          k_MoveFileA },
    { "CopyFileA",          k_CopyFileA },
    { "ExitProcess",        k_ExitProcess },
    { NULL, NULL }
};

void dd_fs_register(void)
{
    win32hle_register(dd_shims_fs);
}
