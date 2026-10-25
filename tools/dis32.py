#!/usr/bin/env python3
# dis32.py VA SIZE — disassembla x86-32 do FalloutNV.exe no VA dado
# mapa: .text raw=0x400 va_rva=0x1000 | .rdata raw=0xBDDA00 rva=0xBDF000 | .data raw=0xD80C00 rva=0xD83000
import sys, capstone

EXE = __import__("os").environ.get("FNV_EXE", "FalloutNV.exe")
BASE = 0x400000

def va_to_file(va):
    rva = va - BASE
    if 0x1000 <= rva < 0x1000 + 0xBDE000:        # .text
        return rva - 0x1000 + 0x400
    if 0xBDF000 <= rva < 0xBDF000 + 0x1A4000:    # .rdata
        return rva - 0xBDF000 + 0xBDDA00
    if 0xD83000 <= rva < 0xD83000 + 0xEF000:     # .data
        return rva - 0xD83000 + 0xD80C00
    raise ValueError(f"VA {va:#x} fora das secoes mapeadas")

va = int(sys.argv[1], 16) if sys.argv[1].startswith("0x") else int(sys.argv[1])
size = int(sys.argv[2], 0)
off = va_to_file(va)
with open(EXE, "rb") as f:
    f.seek(off)
    code = f.read(size)

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False
for ins in md.disasm(code, va):
    print(f"  {ins.address:#010x}: {ins.bytes.hex():20s} {ins.mnemonic} {ins.op_str}")
