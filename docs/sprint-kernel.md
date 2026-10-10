# Sprint — OCCT Kernel WASM

Objectif : exposer `occt-kernel` (handles, primitives, extrude/cut, fillet, tessellation, STEP/BREP échange) sans casser `occt-step`.

Référence produit : [occt-kernel.md](occt-kernel.md).  
Contrat API : [kernel-api.md](kernel-api.md).

## Périmètre

Inclus :

1. Documentation contrat + plan (Phase 0).
2. Target wasm `occt-kernel` et smoke dédié.
3. Ops v1 : primitives → profil/extrude/booléens → fillet/list → I/O échange.
4. Packaging dual (`./step`, `./kernel`) et CI/Release des deux artefacts.
5. Ergonomie Worker (batch, typings, exemple).

Exclus : GCS, feature tree, `TKOpenGl`, PMI writer, naming topologique avancé (`BRepTools_History`), remplacement de `occt-step`.

## Tickets

Milestone : [Sprint OCCT Kernel](https://github.com/naanouff/occt-wasm/milestone/2)

| # | Issue | Titre | Critère de done |
|---|-------|--------|-----------------|
| K0 | [#7](https://github.com/naanouff/occt-wasm/issues/7) | Contrat API kernel | `docs/kernel-api.md` + `docs/occt-kernel.md` mergés ; README pointe dessus |
| K1 | [#8](https://github.com/naanouff/occt-wasm/issues/8) | Arena + primitives + tessellate | Target `occt-kernel` ; smoke box→mesh ; `GAPS-kernel.md` |
| K2 | [#9](https://github.com/naanouff/occt-wasm/issues/9) | Wire / extrude / revolve / booléens | Smoke profil→extrude→cut vert (PR `feature/kernel-solids`) |
| K3 | [#10](https://github.com/naanouff/occt-wasm/issues/10) | Fillet / chamfer + list edges/faces | Ops + doc indices shape-locaux |
| K4 | [#11](https://github.com/naanouff/occt-wasm/issues/11) | STEP/BREP import-export | Échange uniquement ; doctrine rappelée README |
| K5 | [#12](https://github.com/naanouff/occt-wasm/issues/12) | Packaging dual + CI/Release | `package.json` exports ; Release joint step+kernel |
| K6 | [#13](https://github.com/naanouff/occt-wasm/issues/13) | Worker batch + typings + exemple | `examples/kernel-worker.mjs` ; mesures init/mémoire |

## Préparation locale (avant push)

Déjà en place dans le working tree (à committer / PR `feature/kernel-docs` → `develop` pour fermer K0) :

- `docs/occt-kernel.md`, `docs/kernel-api.md`, `docs/sprint-kernel.md`
- Section Kernel du README

## Ordre d’exécution

```text
K0 → K1 → K2 → K3 → K4 → K5 → K6
```

K0 est la gate doc (ce sprint démarre avec K0 livré dans le working tree).  
K5 peut démarrer dès que K1 produit les artefacts ; idéalement après K4 pour joindre I/O aux releases.  
K6 après une API stable (fin K3 minimum).

## Definition of done (sprint)

- [ ] Plan et contrat dans `docs/occt-kernel.md` / `docs/kernel-api.md` ; README à jour
- [ ] Milestone GitHub « Sprint OCCT Kernel » avec issues K0–K6
- [ ] `occt-kernel` build Docker + smoke kernel vert
- [ ] Ops v1 documentées implémentées (primitives → booléens → fillet → I/O)
- [ ] Package / CI exposent step et kernel ; Release `v*` joint les deux wasm

## Notes opérationnelles

- Modeling* déjà ON dans `configure-occt.sh` : K1 porte surtout sur le wrapper C/JS et CMake, pas un rebuild modules OCCT.
- Durée build froid : dizaines de minutes à heures (même pipeline Docker).
- Branches : `feature/kernel-*` → `develop` ([gitflow.md](gitflow.md)).
