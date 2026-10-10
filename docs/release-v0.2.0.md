# Release v0.2.0

Promotion `develop` → `main`, tag `v0.2.0`.

## Contenu

- Artefact **`occt-kernel`** : arena, primitives, wire/extrude/revolve, booléens, fillet/chamfer, list edges/faces, STEP/BREP échange, tessellation.
- Helper **`@naanouff/occt-wasm/kernel/batch`** (`runBatch`), typings, exemple Worker.
- Packaging dual : `./step` et `./kernel`.
- Correctif Docker Windows CRLF (`.gitattributes`, strip CR dans l’image).
- `occt-step` inchangé dans son rôle PMI.

## Doctrine

Historique paramétrique hors bande ; STEP/BREP via le kernel = échange/cache uniquement.

## Validation locale

- Docker Desktop build froid OK.
- `npm run smoke:kernel` : aucun écart (box, extrude-cut, revolve, fillet, BREP/STEP, batch).

## Artefacts Release

- `occt-step.wasm` / `.js`
- `occt-kernel.wasm` / `.js`
- `link-libs.txt`, licences LGPL
