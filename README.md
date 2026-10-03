# occt-wasm

Lecteur STEP AP242 d’[Open CASCADE](https://github.com/Open-Cascade-SAS/OCCT) `V8_0_1` compilé en WebAssembly. L’API appelle `STEPCAFControl_Reader`, `XCAFDoc_DimTolTool` et `XCAFDoc_NotesTool` : maillage, assemblage, datums, tolérances géométriques, cotes et annotations.

Les sources OCCT ne sont pas dans ce dépôt. Le build clone le tag épinglé dans `versions.env`.

Licence du wrapper : LGPL-2.1. OCCT est LGPL-2.1 avec l’exception du fichier `OCCT_LGPL_EXCEPTION.txt`, copié dans `dist/` par l’image Docker.

## Build

```bash
docker build -t occt-wasm .
docker create --name occt-out occt-wasm
docker cp occt-out:/opt/dist ./dist
docker rm occt-out
```

`TKOpenGl` n’est pas compilé : `TKXCAF` est dans le module DataExchange. Visualization et FreeType restent désactivés.

## Smoke

```bash
node scripts/smoke.mjs chemin/nist_ctc_01_asme1_ap242-e1.stp chemin/autre.stp
```

Le rapport `dist/GAPS.md` note la taille du wasm, le temps d’init, et les trous. `MODEL_GEOMETRIC_VIEW` (vues AP242) n’est pas lu.

Le fichier [`ci/build-wasm.yml`](ci/build-wasm.yml) est le job manuel (`workflow_dispatch`). GitHub refuse de le poser dans `.github/workflows` tant que le jeton n’a pas le scope `workflow`. Pour l’activer : copier ce fichier vers `.github/workflows/build-wasm.yml` avec un jeton qui a ce scope.
