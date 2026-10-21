/* DesertDroid — shims de imports por ORDINAL do FalloutNV.exe.
 *
 * O pe_loader (patch 0015) resolve ordinais pela chave "DLL#N". Estes 24
 * sao: wsock32 (21 — jogo singleplayer, sockets nunca usados de verdade),
 * dsound#11 (DirectSoundCreate) e xinput1_3#2/#3. Todas retornam erro
 * benigno para o jogo seguir o caminho "sem rede/sem controle/sem
 * DirectSound". O argc do RET(eax, argc) tem que casar com a funcao real
 * senao a pilha do guest desalinha — os contadores abaixo seguem a
 * tabela winsock 1.1.
 */
#include "win32hle.h"

/* ---- genericos: (valor de retorno, argc) ---- */
static void r_m1_3(void) { RET(0xFFFFFFFFu, 3); }  /* SOCKET_ERROR, 3 args  */
static void r_m1_2(void) { RET(0xFFFFFFFFu, 2); }
static void r_m1_1(void) { RET(0xFFFFFFFFu, 1); }
static void r_m1_4(void) { RET(0xFFFFFFFFu, 4); }
static void r_m1_5(void) { RET(0xFFFFFFFFu, 5); }
static void r_0_0(void)  { RET(0, 0); }
static void r_0_1(void)  { RET(0, 1); }
static void r_0_2(void)  { RET(0, 2); }
static void r_wsaerr(void) { RET(10038, 0); }      /* WSAENOTSOCK          */
static void r_xinput_err(void) { RET(1167, 2); }   /* ERROR_DEVICE_NOT_CONNECTED */
static void r_ds_nodriver(void) { RET(0x8878000A, 3); } /* DirectSound: sem driver */

static const win32hle_shim dd_shims_ordinal[] = {
    /* wsock32 — ordinais Winsock 1.1 (jogo e singleplayer: erros benignos) */
    { "wsock32.dll#1",   r_m1_3 },   /* accept(3)            */
    { "wsock32.dll#2",   r_m1_3 },   /* bind(3)              */
    { "wsock32.dll#3",   r_m1_1 },   /* closesocket(1)       */
    { "wsock32.dll#4",   r_m1_3 },   /* connect(3)           */
    { "wsock32.dll#9",   r_0_1 },    /* htons(1)             */
    { "wsock32.dll#10",  r_0_1 },    /* htonl(1)             */
    { "wsock32.dll#11",  r_0_1 },    /* inet_addr(1)         */
    { "wsock32.dll#12",  r_0_1 },    /* inet_ntoa(1)         */
    { "wsock32.dll#13",  r_m1_3 },   /* ioctlsocket(3)       */
    { "wsock32.dll#15",  r_m1_2 },   /* listen(2)            */
    { "wsock32.dll#16",  r_0_1 },    /* ntohs(1)             */
    { "wsock32.dll#18",  r_0_1 },    /* ntohl(1)             */
    { "wsock32.dll#19",  r_m1_4 },   /* recv(4)              */
    { "wsock32.dll#21",  r_m1_5 },   /* select(5)            */
    { "wsock32.dll#23",  r_m1_4 },   /* send(4)              */
    { "wsock32.dll#52",  r_m1_2 },   /* gethostname(2)       */
    { "wsock32.dll#57",  r_0_2 },    /* WSAStartup(2) -> OK  */
    { "wsock32.dll#101", r_m1_3 },   /* WSAEnumProtocolsA(3) */
    { "wsock32.dll#111", r_0_0 },    /* (verificar argc no log) */
    { "wsock32.dll#115", r_wsaerr }, /* WSAGetLastError()    */
    { "wsock32.dll#151", r_0_0 },    /* (verificar argc no log) */

    /* DirectSound: ordinal 11 = DirectSoundCreate(3) — sem audio por ora */
    { "dsound.dll#11",   r_ds_nodriver },

    /* XInput: ordinais 2/3 (GetState/SetState) — sem controle conectado */
    { "xinput1_3.dll#2", r_xinput_err },
    { "xinput1_3.dll#3", r_xinput_err },

    { NULL, NULL }
};

void dd_shims_register(void)
{
    win32hle_register(dd_shims_ordinal);
}
