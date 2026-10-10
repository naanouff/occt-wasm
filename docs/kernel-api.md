# Contrat API — `occt-kernel`

Surface C/WASM du noyau de modélisation. Plan produit : [occt-kernel.md](occt-kernel.md).

Statut : **contrat v0** (Phase 0). Les symboles listés sont la cible d’implémentation ; seuls `_occt_read_step` / `_occt_free` existent aujourd’hui sur l’artefact `occt-step`.

## Module

| | |
|--|--|
| Factory JS | `createOcctKernel` (`EXPORT_ES6`, `MODULARIZE`) |
| Artefacts | `dist/occt-kernel.js`, `dist/occt-kernel.wasm` |
| Environnement | `worker`, `node` |
| Mémoire | `ALLOW_MEMORY_GROWTH=1` |
| Runtime | `UTF8ToString`, `HEAPU8`, `cwrap`, `ccall` (+ `_malloc` / `_free`) |

Une instance de module = **une arena**. Pas de partage de handles entre workers ni entre instances.

## Doctrine

- L’historique paramétrique vit **hors** de ce module.
- Les handles sont des résultats d’évaluation B-Rep, pas un document.
- STEP / BREP = échange ou cache ; pas de round-trip comme source de vérité.

## Handles et cycle de vie

- Handle : `uint32` opaque, `0` = invalide.
- Ops mutantes **consomment** leurs entrées shape sauf mention contraire ; la sortie est un **nouveau** handle.
- Après `shape_release` ou `arena_clear`, tout usage du handle → `InvalidHandle`.

| Symbole | Signature (conceptuelle) | Rôle |
|---------|--------------------------|------|
| `_occt_shape_release` | `(handle) → void` | Décrémente / libère |
| `_occt_arena_clear` | `() → void` | Vide l’arena |
| `_occt_free` | `(ptr) → void` | Libère une string / buffer alloué par le module |

Les pointeurs de résultat JSON ou binaires sont alloués côté wasm ; l’appelant libère avec `_occt_free`.

## Erreurs

Réponses JSON d’échec :

```json
{ "ok": false, "error": "message lisible", "code": "BooleanFailed" }
```

Codes stables v1 :

| Code | Sens |
|------|------|
| `InvalidHandle` | Handle inconnu ou déjà libéré |
| `InvalidArgument` | Paramètres hors domaine / JSON mal formé |
| `EmptyWire` | Wire sans arêtes utilisables |
| `FaceFailed` | Construction de face impossible |
| `BooleanFailed` | Fuse / cut / common a échoué |
| `FilletFailed` | Filet ou chanfrein a échoué |
| `TessellateFailed` | Maillage impossible |
| `IoFailed` | Import / export STEP ou BREP |
| `InternalError` | `Standard_Failure` non classée |

Succès mesh / listes : `{ "ok": true, ... }`. Succès handle : entier `uint32` non nul (pas de JSON), sauf si l’op renvoie aussi des métadonnées (alors JSON avec `"handle"`).

## Presets de tessellation

Identiques au reader STEP : `coarse` | `normal` (défaut) | `fine`.

## Ops v1

### Primitives

| Symbole | Entrée | Sortie |
|---------|--------|--------|
| `_occt_make_box` | `dx, dy, dz` | handle solid |
| `_occt_make_cylinder` | `radius, height` | handle solid |
| `_occt_make_sphere` | `radius` | handle solid |
| `_occt_make_cone` | `r1, r2, height` | handle solid |

### Profil → solide

Profil plan **XY** en v1 (wires déjà résolus par l’hôte).

| Symbole | Entrée | Sortie |
|---------|--------|--------|
| `_occt_make_wire_polyline` | buffer `xyz` flat (N≥3, fermeture implicite si premier=dernier ou flag) | handle wire |
| `_occt_make_face_from_wire` | handle wire | handle face |
| `_occt_extrude` | handle face/wire, `dx, dy, dz` | handle solid |
| `_occt_revolve` | handle face/wire, axe `(ox,oy,oz, dx,dy,dz)`, `angleRad` | handle solid |

### Booléens

| Symbole | Entrée | Sortie |
|---------|--------|--------|
| `_occt_boolean_fuse` | `a, b` | handle (a et b consommés) |
| `_occt_boolean_cut` | `a, b` | handle |
| `_occt_boolean_common` | `a, b` | handle |

Pas de healing agressif en v1 : échec OCCT → `BooleanFailed`.

### Filets

| Symbole | Entrée | Sortie |
|---------|--------|--------|
| `_occt_fillet_edges` | handle, `radius`, `indices*` (`int32`), `indexCount` (`<0` = all) | handle (entrée consommée) |
| `_occt_chamfer_edges` | handle, `distance`, `indices*`, `indexCount` (`<0` = all) | handle (entrée consommée) |

Les `id` renvoyés par `list_edges` / `list_faces` sont des indices **0-based** dans la shape courante uniquement. Après fillet/chamfer/booléen/extrude, re-lister sur le nouveau handle.

### Introspection (shape courante)

Indices **locaux à la shape** ; invalidés après toute op qui produit un nouveau handle.

| Symbole | Sortie JSON |
|---------|-------------|
| `_occt_list_edges` | `{ ok, edges: [{ id, length, midPoint: [x,y,z] }] }` |
| `_occt_list_faces` | `{ ok, faces: [{ id, area, midPoint: [x,y,z] }] }` |

### Tessellation

| Symbole | Sortie |
|---------|--------|
| `_occt_tessellate` | JSON `{ ok, triangleCount, preset, meshes: [...], wireframe?: [...] }` |

Schéma `meshes` aligné sur le reader STEP (positions, normals, indices, `face` optionnel).

### Échange (pas document de travail)

| Symbole | Entrée | Sortie |
|---------|--------|--------|
| `_occt_import_step` | `bytes`, `length` | handle racine (géométrie seule ; PMI → `occt-step`) |
| `_occt_export_step` | handle, `int* outLength` | `uint8_t*` malloc’d STEP ; libérer avec `_occt_free` |
| `_occt_import_brep` | `bytes`, `length` | handle |
| `_occt_export_brep` | handle, `int* outLength` | `uint8_t*` malloc’d BREP ; libérer avec `_occt_free` |

Échec I/O : handle `0` / pointeur `NULL` et `*outLength = 0`. Pas de consommation du handle à l’export.

## Batch (Phase 6)

Batch **côté JS** (équivalent documenté de `_occt_batch`) : [`js/kernel-batch.mjs`](../js/kernel-batch.mjs) exporté en `@naanouff/occt-wasm/kernel/batch`.

```js
import { runBatch } from '@naanouff/occt-wasm/kernel/batch';

const out = runBatch(occt, [
  { op: 'make_box', args: [10, 20, 30], as: 'body' },
  { op: 'fillet_edges', args: [{ handle: 'body' }, 1.0, null], as: 'filleted' },
  { op: 'tessellate', args: [{ handle: 'filleted' }, 'normal'], as: 'mesh' },
]);
```

- Entrée : tableau ordonné `{ op, args?, as? }`.
- Réfs handle : `{ handle: "alias" }` ou `{ handle: 3 }`.
- Sortie : `{ ok, results, aliases }` ou `{ ok: false, error, code: "BatchFailed", results }`.
- Une instance wasm = une arena ; un Worker = une instance (voir [`examples/kernel-worker.mjs`](../examples/kernel-worker.mjs)).
- Typings : [`types/occt-kernel.d.ts`](../types/occt-kernel.d.ts).

Mesures à noter dans `GAPS-kernel.md` après smoke : `init` ms, taille wasm, et côté Worker `heapBytes` via message `stats`.

## Invariants

1. Handle `0` n’est jamais valide.
2. Une instance wasm = une arena ; pas de IPC de handles.
3. Ops mutantes : entrées shape consommées sauf doc contraire.
4. Indices edges/faces valides uniquement sur le handle listé, avant la prochaine op mutante sur ce solide.
5. `TKOpenGl` / Visualization ne font pas partie du link.

## Alignement hôte (OpenCAD)

Ce contrat est la face wasm d’un `KernelClient` typé côté hôte : l’historique rejoue des ops → handles → tessellate pour la vue ; STEP/BREP pour import/export/cache uniquement.
