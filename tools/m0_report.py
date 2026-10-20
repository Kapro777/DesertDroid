#!/usr/bin/env python3
"""m0_report.py <work_dir> — estatisticas do M0/M1 (sem codigo derivado).

Lê work/catalog.json + work/gen/lift_stats.json e imprime um resumo:
funções, thunks, cobertura de .text, blocos, e stats do lift.
Endereços/tamanhos são metadados factuais (padrão syms.toml da cena recomp);
nenhum código gerado é incluído.
"""
import json, os, sys


def main(work):
    cat = json.load(open(os.path.join(work, "catalog.json")))
    funcs = cat["functions"]
    cs, ce = cat["code_start"], cat["code_end"]
    text_bytes = ce - cs
    total_size = sum(f.get("size", 0) for f in funcs)
    thunks = sum(1 for f in funcs if f.get("is_thunk"))
    blocks = sum(len(f.get("blocks", [])) for f in funcs)
    seed_hits = sum(1 for f in funcs if f.get("entry_kind") == "seed")

    print("=== FNV recomp — relatorio M0/M1 ===")
    print(f"exe:              {cat.get('exe')}")
    print(f"image base:       {cat.get('image_base'):#x}")
    print(f".text range:      {cs:#x}..{ce:#x} ({text_bytes:,} bytes)")
    print(f"funcoes:          {len(funcs):,}")
    print(f"  thunks:         {thunks:,}")
    print(f"  entry via seed: {seed_hits:,}")
    print(f"blocos basicos:   {blocks:,}")
    print(f"tamanho medio:    {total_size/max(len(funcs),1):.0f} bytes/funcao")
    print(f"cobertura .text:  {total_size:,}/{text_bytes:,} = "
          f"{100.0 * total_size / text_bytes:.1f}%")

    ls_path = os.path.join(work, "gen", "lift_stats.json")
    if os.path.exists(ls_path):
        print("--- lift_stats.json ---")
        print(open(ls_path).read())


if __name__ == "__main__":
    main(sys.argv[1])
