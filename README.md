# occt-wasm

Lecteur STEP AP242 d’[Open CASCADE](https://github.com/Open-Cascade-SAS/OCCT) `V8_0_1` compilé en WebAssembly. L’API appelle `STEPCAFControl_Reader`, `XCAFDoc_DimTolTool` et `XCAFDoc_NotesTool` : maillage, assemblage, datums, tolérances géométriques, cotes et annotations.

Les sources OCCT ne sont pas dans ce dépôt. Le build clone le tag épinglé dans `versions.env`.

Licence du wrapper : LGPL-2.1. OCCT est LGPL-2.1 avec l’exception du fichier `OCCT_LGPL_EXCEPTION.txt`, copié dans `dist/` par l’image Docker.

Le JSON renvoie un maillage par face (couleur XCAF lorsqu’elle existe), le filaire des arêtes en paires xyz, l’assemblage, les datums, les tolérances géométriques, les cotes et les annotations. Les positions restent dans les unités du fichier. `MODEL_GEOMETRIC_VIEW` (vues AP242) n’est pas lu. Le module exporte `_occt_read_step` et `_occt_free` ; `cwrap` n’est pas dans le runtime.

## Build

```bash
docker build -t occt-wasm .
docker create --name occt-out occt-wasm
docker cp occt-out:/opt/dist ./dist
docker rm occt-out
```

L’image installe emsdk `6.0.11`. Emscripten `3.1.64` plante à la sélection d’instructions wasm, ou émet un `br_table` que le moteur refuse. `scripts/configure-occt.sh` laisse `OCC_CONVERT_SIGNALS` désactivé pour la même raison. Le lien final passe par `wasm-opt -O1`.

`TKOpenGl` n’est pas compilé : `TKXCAF` est dans le module DataExchange. Visualization et FreeType restent désactivés.

Lorsque le clone dans le conteneur est coupé, `Dockerfile.local` reprend le même build en montant un checkout `OCCT/` déjà au SHA de `versions.env` et les archives emsdk dans `emsdk-cache/` :

```bash
docker build -f Dockerfile.local -t occt-wasm .
```

## Smoke

```bash
node scripts/smoke.mjs chemin/nist_ctc_01_asme1_ap242-e1.stp chemin/autre.stp
node scripts/smoke-kernel.mjs
```

Le rapport `dist/GAPS.md` note la taille du wasm step, le temps d’init, et les trous.  
`dist/GAPS-kernel.md` fait de même pour `occt-kernel` (box → tessellate).

## Gitflow

```text
feature/* | fix/*  →  develop  →  main  →  tag v* (Release)
```

Détail : [`docs/gitflow.md`](docs/gitflow.md).

## CI et Release

Le workflow [`.github/workflows/build-wasm.yml`](.github/workflows/build-wasm.yml) reprend le build Docker local :

- **`workflow_dispatch`** → artifact Actions `occt-step-wasm` (`dist/`)
- **tag `v*`** (ex. `v0.1.0`) → même artifact + GitHub Release avec le wasm, le JS, `link-libs.txt` et les licences

Miroir hors `.github` : [`ci/build-wasm.yml`](ci/build-wasm.yml) (certains jetons sans scope `workflow` ne peuvent pas pousser sous `.github/workflows`). En cas d’écart, `.github/workflows` prime.

Plan et sprint : [`docs/ci-release.md`](docs/ci-release.md), [`docs/sprint-ci-release.md`](docs/sprint-ci-release.md).

## Kernel de modélisation (évolution)

Second artefact `occt-kernel` : arena de shapes, primitives, extrude/booléens/fillet, tessellation.  
**Doctrine** : l’historique paramétrique reste hors bande ; STEP/BREP via le kernel = import/export ou cache (ex. `.cadombrep`), jamais document de travail. Le lecteur `occt-step` reste la voie PMI.

- Plan : [`docs/occt-kernel.md`](docs/occt-kernel.md)
- Contrat API : [`docs/kernel-api.md`](docs/kernel-api.md)
- Sprint : [`docs/sprint-kernel.md`](docs/sprint-kernel.md)
