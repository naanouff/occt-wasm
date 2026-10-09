# Sprint — CI WASM Release

Objectif : activer la CI Docker et publier `dist/` en GitHub Release sur tag `v*`.

Référence produit : [ci-release.md](ci-release.md).

## Périmètre

Inclus :

1. Workflow `.github/workflows/build-wasm.yml` (triggers, free-disk, artifact, release).
2. Documentation README + `docs/`.
3. Validation `workflow_dispatch` puis tag de test.

Exclus : smoke CI, cache Docker, npm publish, self-hosted runner.

## Tickets

Milestone : [Sprint CI Release](https://github.com/naanouff/occt-wasm/milestone/1)

| # | Issue | Titre | Critère de done |
|---|-------|--------|-----------------|
| S1 | [#1](https://github.com/naanouff/occt-wasm/issues/1) | Activer le workflow sous `.github/workflows` | Fichier commit/poussé ; README pointe vers lui ; `ci/build-wasm.yml` aligné |
| S2 | [#2](https://github.com/naanouff/occt-wasm/issues/2) | Triggers + release sur tag `v*` | `workflow_dispatch` + `push.tags: v*` ; `softprops/action-gh-release` joint les 5 fichiers `dist/` |
| S3 | [#3](https://github.com/naanouff/occt-wasm/issues/3) | Free disk avant build | Step avant `docker build` ; build ne tombe pas pour disque plein sur un run froid |
| S4 | [#4](https://github.com/naanouff/occt-wasm/issues/4) | Validation end-to-end | Un run manuel produit l’artifact ; un tag `v*` produit une Release GitHub |

## Préparation locale (avant push)

Déjà en place dans le working tree (à committer / pousser pour ouvrir S4) :

- `docs/ci-release.md`, `docs/sprint-ci-release.md`, section CI du README
- `.github/workflows/build-wasm.yml` et miroir `ci/build-wasm.yml` (triggers, free-disk, artifact, release)

## Ordre d’exécution

```text
S1 → S2 → S3 → S4
```

S2 et S3 peuvent être livrés dans le même commit de workflow ; S4 démarre après push sur `main` (ou branche trackée par Actions).

## Definition of done (sprint)

- [ ] Documentation du plan dans `docs/ci-release.md` et section CI du README
- [ ] Workflow prêt (triggers, free-disk, artifact, release conditionnelle)
- [ ] Milestone GitHub « Sprint CI Release » avec issues S1–S4
- [ ] Run `workflow_dispatch` vert + Release sur tag `v*`

## Notes opérationnelles

- Push sous `.github/workflows` : jeton avec scope `workflow` si GitHub le refuse.
- Durée attendue : dizaines de minutes à quelques heures (build OCCT froid).
- Après validation, fermer les issues et le milestone.
