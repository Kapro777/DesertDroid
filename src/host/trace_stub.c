/* DesertDroid — stubs de trace (o recomp_trace.c do pcrecomp e Windows-only;
 * nada no caminho win32hle o referencia. Se algo do codigo gerado pedir um
 * destes simbolos no link, os no-ops abaixo cobrem sem alterar o modelo.) */
#include <stdint.h>

void recomp_trace_enter(uint32_t va) { (void)va; }
void recomp_dump_trace(const char *why) { (void)why; }
void recomp_trace_flush(void) {}
void recomp_trace_help(void) {}
int  recomp_trace_arg(int argc, char **argv, int i) { (void)argc; (void)argv; return i; }
