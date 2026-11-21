# Release v0.3.0

Promotion `develop` → `main`, tag `v0.3.0`.

## Contenu

- `_occt_sweep_helix` : solide obtenu en balayant un fil fermé le long d’une hélice (une arête par tour, pas `> 0` = sens droit).
- `_occt_make_wire_arcs` : fil fermé de segments et d’arcs par trois points.
- `list_edges` ajoute les extrémités et `line`.
- Tessellation : flèche relative à la pièce, normales de sommet lues sur la surface.
- Image Docker : OCCT est compilé avant la copie des sources du wrapper, pour relinker sans reconstruire OCCT.

## Doctrine

Historique paramétrique hors bande ; STEP/BREP via le kernel = échange/cache uniquement. Le filet de vis est un solide produit par `sweep_helix`, pas un maillage hôte.

## Artefacts Release

- `occt-step.wasm` / `.js`
- `occt-kernel.wasm` / `.js`
- `link-libs.txt`, licences LGPL
