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

- [x] M0 — Catálogo: 119.800 funções, 2.078.307 blocos (runner, ~6 min)
- [x] M1 — Lift: 0 falhas, 28,9M linhas de C em 400 arquivos
- [ ] M2 — Compilar+linkar host (win32hle) — rodando na CI
- [ ] M3 — Boot permissivo + bring-up de imports (ver docs/M3_IMPORT_CENSUS.md)
- [ ] M4 — Primeiro frame / menu (D3D9→Vulkan via DXVK-Native, Bink, DirectSound/AAudio)
- [ ] M5 — APK: Gradle + NDK, touch controls, loader de assets do usuário
- [ ] M6 — Gameplay mínimo: novo jogo, Goodsprings, save/load

## Launcher (roadmap)

- V1: detectar pasta do jogo no aparelho (BYOG manual)
- V2: **login Steam do próprio usuário** (Steam Guard incluso) e download da
  cópia que ele já possui (AppID 22380) direto dos servidores da Valve —
  modelo GDroid. Zero arquivos distribuídos pela engine; aquisição limpa
  com binário oficial intacto.
- V3: **aba de Mods**:
  - instação local (zip/7z → `Data/`), gerenciador de load order (estilo
    Droid Mod Loader) — primeiro, 100% seguro;
  - "fonte de mods" genérica e extensível que usa a chave de API pessoal do
    próprio usuário e NUNCA re-hospeda arquivos (download sempre sai do
    servidor original). UI genérica, sem branding de site.
  - Nota honesta: mods dependentes de NVSE não funcionam no estágio inicial
    (NVSE injeta no exe; suporte no recomp é projeto futuro).

## Estado

Em andamento — M0/M1 (catálogo + lift) rodando via GitHub Actions (repo privado `fnv-build-inputs` segura o exe BYOG).
Referências: external/pcrecomp (pinned 35548b8 + 14 patches nfsu2),
external/nfsu2-recompiled (playbook Android), external/fnv-macos (difftest).

**Divisão de trabalho**: tarefas leves (edição, scripts, análise) rodam no
dispositivo; compilação e lift pesados rodam no GitHub Actions.
