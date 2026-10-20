#!/usr/bin/env python3
"""merge_seeds.py <exe> <out.json> <seed files...>

Merges pcrecomp seed files (vtable_scan --seeds, rtti --seeds, ...) plus
TLS callbacks from the PE into one seeds_all.json for run_lift.py catalog.
"""
import json, sys, pefile


def load_seed_addresses(path):
    data = json.load(open(path))
    if isinstance(data, dict):
        for key in ("seeds", "methods", "addresses"):
            if key in data and isinstance(data[key], list):
                data = data[key]
                break
    out = set()
    for e in data:
        if isinstance(e, dict):
            a = e.get("address", e.get("va", e.get("addr")))
        else:
            a = e
        if a is not None:
            out.add(int(a, 0) if isinstance(a, str) else int(a))
    return out


def main():
    exe, out_path = sys.argv[1], sys.argv[2]
    all_addrs = set()
    for p in sys.argv[3:]:
        n = len(all_addrs)
        all_addrs |= load_seed_addresses(p)
        print(f"  {p}: +{len(all_addrs) - n}")
    try:
        pe = pefile.PE(exe, fast_load=True)
        pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_TLS"]])
        if hasattr(pe, "DIRECTORY_ENTRY_TLS"):
            tlscb = pe.DIRECTORY_ENTRY_TLS.struct.AddressOfCallBacks
            base = pe.OPTIONAL_HEADER.ImageBase
            data = pe.get_data(tlscb - base, 64)
            off = 0
            while off < len(data):
                import struct
                va = struct.unpack_from("<I", data, off)[0]
                if va == 0:
                    break
                all_addrs.add(va)
                print(f"  TLS callback: {va:#x}")
                off += 4
    except Exception as e:
        print(f"  (no TLS callbacks: {e})")
    doc = [{"address": a} for a in sorted(all_addrs)]
    json.dump(doc, open(out_path, "w"))
    print(f"wrote {out_path}: {len(all_addrs)} seeds")


if __name__ == "__main__":
    main()
