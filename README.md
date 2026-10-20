# DesertDroid

**Fallout: New Vegas nativo no Android (ARM64)** — engine/porte por static recompilation, no modelo GDroid/fallout2-ce.

Porte nativo (ARM64/Android) de Fallout: New Vegas (PC, x86-32) por
**static recompilation** — o mesmo modelo do porte de BLACK (xboxrecomp),
nfsu2-recompiled e CallofRecomp.

## Modelo legal (bring-your-own-game)

- Este repositório **não contém e nunca vai conter** código, executáveis,
  DLLs ou assets do jogo — nada derivado do FalloutNV.exe é distribuído.
- Todo código gerado (catálogo, C recompilado, artefatos Ghidra) fica em
  `work/` (gitignored) e é produzido localmente/na CI a partir da cópia
  do **próprio usuário**.
- Nossa licença (MIT) cobre apenas o código autoral: tooling, runtime,
  camada Android, patches.

## Pipeline

```
FalloutNV.exe (do usuário)
  → pcrecomp (vtable_scan + rtti + disasm32 recursive descent)   [seeds: 32.683]
  → lift32 (x86-32 → C, x87/MMX/SSE/SSE2)
  → clang/NDK arm64 + runtime (win32hle + DXVK-Native + libvorbis + stub steam)
  → APK Android (Vulkan, touch controls)
```

## Roadmap

- [ ] M0 — Catálogo: recursive descent do `.text` (12,4 MB), medir funções
      e taxa de cobertura dos seeds
- [ ] M1 — Lift: gerar C de todas as funções catalogadas, 0 falhas,
      difftest de amostras contra Unicorn
- [ ] M2 — Compilar+linkar host Linux/AArch64 (PRoot) com runtime mínimo
- [ ] M3 — Boot: WinMain alcançado, janela criada, D3D9 init via DXVK-Native
- [ ] M4 — Primeiro frame / menu (Bink HLE, DirectSound/AAudio)
- [ ] M5 — APK: Gradle + NDK, touch controls, loader de assets do usuário
- [ ] M6 — Gameplay mínimo: novo jogo, Goodsprings, save/load

## Estado

Em andamento — M0/M1 (catálogo + lift) rodando via GitHub Actions (repo privado `fnv-build-inputs` segura o exe BYOG).
Referências: external/pcrecomp (pinned 35548b8 + 14 patches nfsu2),
external/nfsu2-recompiled (playbook Android), external/fnv-macos (difftest).

**Divisão de trabalho**: tarefas leves (edição, scripts, análise) rodam no
dispositivo; compilação e lift pesados rodam no GitHub Actions.
