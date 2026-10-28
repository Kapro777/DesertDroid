/* DesertDroid — lote locale/codepage (init do CRT MSVC).
 *
 * O CRT valida codepages e locale do usuario antes do _initterm. Respondemos
 * com um locale "English (US)" consistente: o jogo e a edicao inglesa e os
 * textos/ordenacao seguem em (ASCII) para o boot. CompareString/LCMapString
 * cobrem as flags comuns; o resto retorna falha benigna.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include "win32hle.h"

#define DD_LCID_EN_US 0x0409u

#define C1_UPPER   0x0001u
#define C1_LOWER   0x0002u
#define C1_DIGIT   0x0004u
#define C1_SPACE   0x0008u
#define C1_PUNCT   0x0010u
#define C1_CNTRL   0x0020u
#define C1_BLANK   0x0040u
#define C1_XDIGIT  0x0080u
#define C1_ALPHA   0x0100u
#define C1_DEFINED 0x0200u

/* GetStringTypeA/W(dwInfoType, source, count, output).
 * O CRT VS2008 usa CT_CTYPE1 durante o parse de locale/argv. O shim antigo
 * apenas devolvia TRUE sem escrever output; isso deixa a stack local
 * indefinida e não cumpre o contrato do Win32. Para a edição inglesa, a
 * classificação ASCII é suficiente para a fase de arranque.
 */
static uint16_t dd_ctype1(uint32_t ch)
{
    uint16_t t = C1_DEFINED;
    if (ch >= 'A' && ch <= 'Z') t |= C1_UPPER | C1_ALPHA;
    if (ch >= 'a' && ch <= 'z') t |= C1_LOWER | C1_ALPHA;
    if (ch >= '0' && ch <= '9') t |= C1_DIGIT;
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ||
        ch == '\v' || ch == '\f') t |= C1_SPACE;
    if (ch == ' ' || ch == '\t') t |= C1_BLANK;
    if (ch <= 0x1Fu || (ch >= 0x7Fu && ch <= 0x9Fu)) t |= C1_CNTRL;
    if (ch <= 0x7Fu && ispunct((unsigned char)ch)) t |= C1_PUNCT;
    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') ||
        (ch >= 'a' && ch <= 'f')) t |= C1_XDIGIT;
    return t;
}

static void k_GetStringTypeA(void)
{
    uint32_t kind = A32(0);
    const unsigned char *src = (const unsigned char *)APTR(1);
    int32_t count = (int32_t)A32(2);
    uint16_t *dst = (uint16_t *)APTR(3);
    if (kind != 1u || !src || !dst) RET(0, 4);
    uint32_t n = 0;
    if (count < 0) {
        while (n < 0x100000u && src[n] != 0) n++;
        if (n == 0x100000u) RET(0, 4);
        n++; /* cchSrc < 0: include terminating NUL */
    } else {
        n = (uint32_t)count;
    }
    for (uint32_t i = 0; i < n; i++) dst[i] = dd_ctype1(src[i]);
    RET(1, 4);
}

static void k_GetStringTypeW(void)
{
    uint32_t kind = A32(0);
    const uint16_t *src = (const uint16_t *)APTR(1);
    int32_t count = (int32_t)A32(2);
    uint16_t *dst = (uint16_t *)APTR(3);
    if (kind != 1u || !src || !dst) RET(0, 4);
    uint32_t n = 0;
    if (count < 0) {
        while (n < 0x100000u && src[n] != 0) n++;
        if (n == 0x100000u) RET(0, 4);
        n++; /* cchSrc < 0: include terminating NUL */
    } else {
        n = (uint32_t)count;
    }
    for (uint32_t i = 0; i < n; i++) dst[i] = dd_ctype1(src[i]);
    RET(1, 4);
}

/* ---- codepage ---- */
static void k_IsValidCodePage(void) { RET(1, 1); }        /* tudo valido   */
static void k_GetConsoleCP(void)     { RET(437, 0); }
static void k_GetConsoleOutputCP(void) { RET(437, 0); }
static void k_GetConsoleMode(void)   { RET(0, 2); }       /* nao-console    */

/* ---- locale ---- */
static void k_IsValidLocale(void)     { RET(1, 2); }
static void k_GetUserDefaultLCID(void) { RET(DD_LCID_EN_US, 0); }
static void k_GetSystemDefaultLCID(void) { RET(DD_LCID_EN_US, 0); }
static void k_GetUserDefaultLangID(void) { RET(0x0409u & 0x3FFu, 0); }
static void k_GetUserDefaultUILanguage(void) { RET(0x0409u, 0); }

typedef struct { uint32_t type; const char *val; } loc_entry;
static const loc_entry k_locinfo[] = {
    { 0x00000002, "English (United States)" },  /* LOCALE_SLANGUAGE      */
    { 0x00001001, "English" },                  /* LOCALE_SENGLANGUAGE    */
    { 0x00001002, "United States" },            /* LOCALE_SENGCOUNTRY     */
    { 0x00000004, "." },                        /* LOCALE_SDECIMAL        */
    { 0x00000003, "," },                        /* LOCALE_STHOUSAND       */
    { 0x00000005, "2" },                        /* LOCALE_IDIGITS         */
    { 0x0000001C, "/" },                       /* LOCALE_SDATE           */
    { 0x0000001D, ":" },                       /* LOCALE_STIME           */
    { 0x00000028, "M/d/yyyy" },                 /* LOCALE_SSHORTDATE      */
    { 0x00000029, "dddd, MMMM dd, yyyy" },      /* LOCALE_SLONGDATE       */
    { 0x00000022, "AM" },                       /* LOCALE_S1159           */
    { 0x00000023, "PM" },                       /* LOCALE_S2359           */
    { 0x0000000F, "-" },                       /* LOCALE_SLIST           */
    { 0x00001007, "1252" },                    /* LOCALE_IDEFAULTANSICODEPAGE */
    { 0x0000100A, "437" },                     /* LOCALE_IDEFAULTCODEPAGE     */
    { 0, NULL }
};

/* GetLocaleInfoA(LCID, LCTYPE, LPSTR, cch) */
static void k_GetLocaleInfoA(void)
{
    uint32_t lctype = A32(1);
    char *buf = (char *)APTR(2);
    uint32_t cch = A32(3);
    const char *val = NULL;
    if (lctype & 0x80000000u) { /* LOCALE_RETURN_CONSTANTS etc: base flag */
        RET(0, 4);
    }
    for (const loc_entry *e = k_locinfo; e->val; e++)
        if (e->type == (lctype & 0xFFFFF)) { val = e->val; break; }
    if (!val) RET(0, 4);
    if (!buf || cch == 0) RET((uint32_t)strlen(val) + 1, 4);
    snprintf(buf, cch, "%s", val);
    RET((uint32_t)strlen(buf) + 1, 4);
}

/* GetLocaleInfoW(LCID, LCTYPE, LPWSTR, cch) — mesma tabela em UTF-16 */
static void k_GetLocaleInfoW(void)
{
    uint32_t lctype = A32(1);
    uint16_t *buf = (uint16_t *)APTR(2);
    uint32_t cch = A32(3);
    const char *val = NULL;
    for (const loc_entry *e = k_locinfo; e->val; e++)
        if (e->type == (lctype & 0xFFFFF)) { val = e->val; break; }
    if (!val) RET(0, 4);
    uint32_t n = (uint32_t)strlen(val);
    if (!buf || cch == 0) RET(n + 1, 4);
    for (uint32_t i = 0; i < n && i + 1 < cch; i++)
        buf[i] = (uint16_t)(unsigned char)val[i];
    if (cch) buf[n < cch ? n : cch - 1] = 0;
    RET(n + 1, 4);
}

/* LCMapStringA(Locale, dwFlags, lpSrc, cchSrc, lpDest, cchDest) */
static void k_LCMapStringA(void)
{
    uint32_t flags = A32(1);
    const char *src = (const char *)APTR(2);
    uint32_t cchSrc = A32(3);
    char *dst = (char *)APTR(4);
    uint32_t cchDst = A32(5);
    if (!src) RET(0, 6);
    uint32_t n = cchSrc ? cchSrc : (uint32_t)strlen(src) + 1;
    if (!dst || cchDst == 0) RET(n, 6);
    if (n > cchDst) n = cchDst;
    for (uint32_t i = 0; i < n; i++) {
        char c = src[i];
        if ((flags & 0x00000100u) /* LCMAP_LOWERCASE */) c = (char)tolower((unsigned char)c);
        if ((flags & 0x00000200u) /* LCMAP_UPPERCASE */)  c = (char)toupper((unsigned char)c);
        dst[i] = c;
    }
    RET(n, 6);
}

/* LCMapStringW — mesmo, UTF-16 */
static void k_LCMapStringW(void)
{
    uint32_t flags = A32(1);
    const uint16_t *src = (const uint16_t *)APTR(2);
    uint32_t cchSrc = A32(3);
    uint16_t *dst = (uint16_t *)APTR(4);
    uint32_t cchDst = A32(5);
    if (!src) RET(0, 6);
    uint32_t n = cchSrc;
    if (n && src[n - 1] == 0) n--;             /* cch inclui nulo: mapeia conteudo */
    if (!dst || cchDst == 0) RET(n + 1, 6);
    if (n > cchDst) n = cchDst;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t c = src[i];
        if (flags & 0x00000100u) c = (uint16_t)towlower(c);
        if (flags & 0x00000200u) c = (uint16_t)towupper(c);
        dst[i] = c;
    }
    RET(n + 1, 6);
}

/* CompareStringA/W(Locale, dwCmpFlags, s1, cch1, s2, cch2) -> 1/2/3 */
static int cmp_run(const char *a, uint32_t na, const char *b, uint32_t nb,
                   int wide, uint32_t flags)
{
    (void)flags;
    int r = 0;
    uint32_t n = na < nb ? na : nb;
    for (uint32_t i = 0; i < n && !r; i++) {
        uint32_t ca = wide ? ((const uint16_t *)a)[i] : (unsigned char)a[i];
        uint32_t cb = wide ? ((const uint16_t *)b)[i] : (unsigned char)b[i];
        r = ca < cb ? -1 : (ca > cb ? 1 : 0);
    }
    if (!r) r = na < nb ? -1 : (na > nb ? 1 : 0);
    return r;
}
static void k_CompareStringA(void)
{
    const char *a = (const char *)APTR(2); uint32_t na = A32(3);
    const char *b = (const char *)APTR(4); uint32_t nb = A32(5);
    int r = cmp_run(a, na ? na : (uint32_t)strlen(a), b, nb ? nb : (uint32_t)strlen(b), 0, 0);
    RET(r < 0 ? 1u : (r ? 3u : 2u), 6);
}
static void k_CompareStringW(void)
{
    const uint16_t *a = (const uint16_t *)APTR(2); uint32_t na = A32(3);
    const uint16_t *b = (const uint16_t *)APTR(4); uint32_t nb = A32(5);
    int r = cmp_run((const char *)a, na, (const char *)b, nb, 1, 0);
    RET(r < 0 ? 1u : (r ? 3u : 2u), 6);
}

/* EnumSystemLocalesA(lpEnumProc, dwFlags): chama o callback do guest uma
 * vez com "00000409" e encerra (suficiente pro CRT validar locale). */
static void k_EnumSystemLocalesA(void)
{
    uint32_t proc = A32(0);
    static char loc[] = "00000409";
    uint32_t arg = (uint32_t)(uintptr_t)loc;
    if (proc) hle_call_guest(proc, 1, &arg);
    RET(1, 2);
}

static void k_SetEnvironmentVariableA(void) { RET(1, 2); }

static const win32hle_shim dd_shims_locale[] = {
    { "GetStringTypeA",      k_GetStringTypeA },
    { "GetStringTypeW",      k_GetStringTypeW },
    { "IsValidCodePage",     k_IsValidCodePage },
    { "GetConsoleCP",        k_GetConsoleCP },
    { "GetConsoleOutputCP",  k_GetConsoleOutputCP },
    { "GetConsoleMode",      k_GetConsoleMode },
    { "IsValidLocale",       k_IsValidLocale },
    { "GetUserDefaultLCID",  k_GetUserDefaultLCID },
    { "GetSystemDefaultLCID", k_GetUserDefaultLCID },
    { "GetUserDefaultLangID", k_GetUserDefaultLangID },
    { "GetUserDefaultUILanguage", k_GetUserDefaultUILanguage },
    { "GetLocaleInfoA",      k_GetLocaleInfoA },
    { "GetLocaleInfoW",      k_GetLocaleInfoW },
    { "LCMapStringA",        k_LCMapStringA },
    { "LCMapStringW",        k_LCMapStringW },
    { "CompareStringA",      k_CompareStringA },
    { "CompareStringW",      k_CompareStringW },
    { "EnumSystemLocalesA",  k_EnumSystemLocalesA },
    { "SetEnvironmentVariableA", k_SetEnvironmentVariableA },
    { NULL, NULL }
};

void dd_locale_register(void)
{
    win32hle_register(dd_shims_locale);
}
