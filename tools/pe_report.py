#!/usr/bin/env python3
"""pe_report.py — PE triage for FalloutNV.exe. Stdlib only.

Reports: machine, LAA flag, entrypoint location, sections (+entropy),
data directories, imports, overlay, and known DRM wrapper signatures.
"""
import struct, sys, math, datetime

DIR_NAMES = {
    0: "EXPORT", 1: "IMPORT", 2: "RESOURCE", 3: "EXCEPTION", 4: "CERTIFICATE",
    5: "RELOCATION", 6: "DEBUG", 7: "ARCH", 8: "GLOBALPTR", 9: "TLS",
    10: "LOAD_CONFIG", 11: "BOUND_IMPORT", 12: "IAT", 13: "DELAY_IMPORT",
    14: "CLR", 15: "RESERVED",
}

DRM_SECTIONS = {
    b".bind": "Steam DRM wrapper",
    b".themida": "Themida",
    b".aspack": "ASPack",
    b".UPX0": "UPX", b".UPX1": "UPX",
    b".vmp0": "VMProtect", b".vmp1": "VMProtect",
    b".enigma1": "Enigma",
}


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    freq = [0] * 256
    for b in data:
        freq[b] += 1
    n = len(data)
    e = 0.0
    for c in freq:
        if c:
            p = c / n
            e -= p * math.log2(p)
    return e


def rva_to_off(rva, sections):
    for name, vaddr, vsize, raddr, rsize in sections:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            if raddr == 0:
                return None
            return raddr + (rva - vaddr)
    return None


def cstr(d, off):
    end = d.find(b"\0", off)
    return d[off:end].decode("latin1", "replace")


def main(path):
    d = open(path, "rb").read()
    print(f"file: {path}")
    print(f"size: {len(d):,} bytes")
    if d[:2] != b"MZ":
        sys.exit("not an MZ file")
    e_lfanew = struct.unpack_from("<I", d, 0x3C)[0]
    if d[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        sys.exit("no PE signature")
    coff = e_lfanew + 4
    machine, nsec, tds, _psym, _nsym, optsize, chars = struct.unpack_from(
        "<HHIIIHH", d, coff)
    mach = {0x14C: "i386 (x86-32)", 0x8664: "AMD64"}.get(machine, hex(machine))
    print(f"machine: {mach}   sections: {nsec}   characteristics: {chars:#x}"
          + ("   [LARGE_ADDRESS_AWARE]" if chars & 0x20 else "   [NOT LAA]"))
    try:
        ts = datetime.datetime.fromtimestamp(tds, datetime.timezone.utc)
        print(f"link timestamp: {ts} UTC")
    except Exception:
        print(f"link timestamp: {tds}")

    opt = coff + 20
    magic = struct.unpack_from("<H", d, opt)[0]
    if magic != 0x10B:
        sys.exit(f"not PE32 (magic {magic:#x}) — expected 32-bit x86")
    (magic, _lmaj, _lmin, codesize, initdata, _uninit, ep, _bcode, _bdata,
     imgbase, secalign, _falign) = struct.unpack_from("<HBBIIIIIIIII", d, opt)
    subsys = struct.unpack_from("<H", d, opt + 68)[0]
    numdirs = struct.unpack_from("<I", d, opt + 92)[0]
    dllchars = struct.unpack_from("<H", d, opt + 70)[0]
    print(f"entrypoint RVA: {ep:#x}   image base: {imgbase:#x}   "
          f"subsystem: {subsys} (2=GUI)   dllchars: {dllchars:#x}"
          + ("  [DYNAMIC_BASE/ASLR]" if dllchars & 0x40 else ""))

    print("data directories:")
    dirs = {}
    for i in range(min(numdirs, 16)):
        rva, size = struct.unpack_from("<II", d, opt + 96 + 8 * i)
        if rva or size:
            dirs[i] = (rva, size)
            print(f"  [{i:2d}] {DIR_NAMES.get(i, '?'):13s} rva={rva:#010x} size={size:#x}")

    secoff = opt + optsize
    sections = []
    print("sections:")
    ep_section = None
    for i in range(nsec):
        raw_name, vsize, vaddr, rsize, raddr = struct.unpack_from(
            "<8sIIII", d, secoff + 40 * i)
        sflags = struct.unpack_from("<I", d, secoff + 40 * i + 36)[0]
        name = raw_name.rstrip(b"\0")
        ent = entropy(d[raddr:raddr + min(rsize, 1 << 21)])
        exec_bit = "X" if sflags & 0x20000000 else " "
        sections.append((name, vaddr, vsize, raddr, rsize))
        warn = ""
        if name in DRM_SECTIONS:
            warn = f"  << {DRM_SECTIONS[name]}"
        if vaddr <= ep < vaddr + vsize:
            ep_section = name
        print(f"  {name.decode('latin1'):8s} vaddr={vaddr:#010x} vsize={vsize:#010x} "
              f"raw={rsize:#010x} {exec_bit}{ent:4.2f} entropy{warn}")
    print(f"entrypoint lives in section: {ep_section}")
    if ep_section not in (b".text", b"CODE"):
        print("  !! entrypoint outside .text — possible packer/DRM wrapper")

    # overlay = bytes after last raw section
    end_raw = max((raddr + rsize) for _n, _va, _vs, raddr, rsize in sections)
    if len(d) > end_raw:
        print(f"overlay: {len(d) - end_raw:,} bytes after last section "
              f"(entropy {entropy(d[end_raw:end_raw + (1 << 20)]):.2f})")

    # imports
    if 1 in dirs:
        rva, size = dirs[1]
        off = rva_to_off(rva, sections)
        print("imports:")
        total_funcs = 0
        pos = off
        while True:
            oft, tds2, fc, name_rva, iat = struct.unpack_from("<IIIII", d, pos)
            if oft == tds2 == fc == name_rva == iat == 0:
                break
            dn = cstr(d, rva_to_off(name_rva, sections))
            thunk_rva = oft or iat
            toff = rva_to_off(thunk_rva, sections)
            cnt = 0
            while True:
                val = struct.unpack_from("<I", d, toff + 4 * cnt)[0]
                if val == 0:
                    break
                cnt += 1
            total_funcs += cnt
            print(f"  {dn:24s} {cnt:3d} functions")
            pos += 20
        print(f"  total imported functions: {total_funcs}")


if __name__ == "__main__":
    main(sys.argv[1])
