#!/usr/bin/env python3
"""va_probe.py <exe> <va> — diagnostico local de um crash do DesertDroid.

Dado o VA (sub_XXXXXXXX) reportado pelo crash-report, disassembla a regiao
com capstone, anota chamadas (alvos e imports via IAT), acesso a FS: (TIB,
TLS em fs:[0x2C]) e padroes de TLS estatico — tudo direto no exe original.
"""
import sys, struct
sys.path.insert(0, "/home/tiny/vegas/external/pcrecomp/tools/pe")
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from pe_analyze import analyze_pe, build_iat_map


def main(exe, va):
    va = int(va, 0)
    info = analyze_pe(exe)
    iat = build_iat_map(info)   # {slot_va: "dll!name"}
    data = open(exe, "rb").read()
    base = info.image_base
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    off = va - base
    print(f"exe: {exe}  base: {base:#x}  va: {va:#x}  file off: {off:#x}")
    in_text = info.code_start <= va < info.code_end
    print(f"dentro de .text: {in_text} (code {info.code_start:#x}..{info.code_end:#x})")

    start = max(info.code_start, va - 96)
    code = data[start - base: (va + 160) - base]
    print(f"--- disassembly de {start:#x} ate {va + 160:#x} (* = o VA do crash):")
    for ins in md.disasm(code, start):
        mark = "  * " if ins.address == va else "    "
        extra = ""
        if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("dword ptr ["):
            # chamada via IAT: [slot] -> importa
            try:
                slot = int(ins.op_str.split("[")[1].rstrip("]"), 16)
                if slot in iat:
                    extra = f"   ; IAT -> {iat[slot]}"
            except ValueError:
                pass
        if "fs:" in ins.op_str:
            extra += "   ; FS: (TIB/TLS)"
        print(f"{mark}{ins.address:#x}: {ins.mnemonic} {ins.op_str}{extra}")

    print("--- acessos a fs:[0x2C] (array TLS) nas proximidades:",
          "SIM (suspeita de TLS estatico)" if any("0x2c" in ins.op_str for ins in md.disasm(code, start)) else "nao")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
