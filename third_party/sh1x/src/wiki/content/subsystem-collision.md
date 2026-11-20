---
slug: subsystem-collision
title: Collision and physics
space: subsystems
summary: src/codol/collision — reading a CoDOL BSP into owned structures, the resolver pattern that keeps it honest, dynamic entities, triggers and glass.
position: 70
tags: [collision]
---

This area turns a CoDOL map's collision and world assets into structures the
retained clip-map code can work with. It is written almost entirely as **pure
readers**: they parse, they copy, they return owned data — and they explicitly
do not install anything into the engine.

That separation is the design. Every reader's comment says some version of
"this does not install a clipMap", "this is not engine publication", "failure
preserves `out`". Staging and publication are different steps so that a map that
fails to parse halfway through cannot leave a half-built world behind.

## The staged map

```c
typedef struct {
    codol_collision_materials_t     materials;
    codol_collision_brushes_t       brushes;
    codol_collision_terrain_t       terrain;
    codol_collision_leaves_t        leaves;
    codol_collision_brush_tree_t    brush_tree;
    codol_map_entities_t            entities;
    uint32_t                        static_model_count;
    codol_collision_static_models_t static_models;
    uint32_t                        checksum;
} codol_collision_map_t;
```

```c
int CODOL_ReadCollisionMap(const codol_asset_t *collision, const codol_asset_t *world,
                           codol_collision_name_resolver_t resolver, void *context,
                           codol_collision_map_t *out);
int CODOL_LoadMountedCollision(const char *map_name, codol_collision_map_t *out);
```

The contract, repeated for every component: `out` must be zero-initialised or a
previous result; **success frees and replaces it, failure preserves it**, and a
failure part-way frees every component it had already staged.

The collision and world assets **must come from the same zone**, because pooled
references are zone-local. `CODOL_FindZoneCollisionPair` distinguishes "no
matching inline world basename" (0) from "invalid or ambiguous pair" (-1), and
pooled basenames are resolved through shared strings rather than guessed.

## Why there are resolver callbacks

Two things a BSP references cannot be resolved by the parser itself: material
names and static-model XModels. Both are passed in as callbacks, and both leave
a visible trace when they fail:

```c
typedef ... codol_collision_name_resolver_t;   /* material names */
typedef ... codol_collision_model_resolver_t;  /* native XModels */
```

- Unresolved references are **counted**, not silently dropped:
  `materials.unresolved` and `static_models.unresolved`. The rule is stated
  plainly — *never install while `unresolved` is nonzero*.
- Resolver strings are copied immediately; resolved XModels stay
  **resolver-owned and must outlive the map**.
- A NULL resolution retains the explicit reference rather than substituting
  anything.

Names must fit the retained 64-byte engine field, which is checked rather than
truncated.

## Component notes worth knowing

**Brushes.** `CODOL_ReadCollisionBrushes` takes the shared plane array
explicitly — exactly `planeCount * 20` bytes. There is **no heuristic search and
no fabricated plane**; resolving the shared array is the caller's job, which is
what `CODOL_ReadWorldBrushes` does after matching map identity and counts.

**Leaves.** CoDOL leaf records have **no IW3 visibility-cluster field**. The
converted cluster stays `-1` until visibility integration supplies it, and
leaf-surface values are preserved exactly as serialized rather than clamped to a
guessed renderer index limit.

**Terrain.** Accepts the forward-ordered trees and contiguous partition border
slices measured in shipped maps; unsupported relationships fail atomically
rather than being interpreted optimistically.

**World planes.** `CODOL_ReadWorldPlanes` is a structural traversal of the
graphics-world prefix (including inline sky images) that returns a **borrowed
span** and loads no renderer resources.

## The rest of the area

| File | What it owns |
|---|---|
| `codol_dynents.c`, `codol_dynents_native.c` | Dynamic entities — the destructible and movable props the BSP carries. |
| `codol_trigger_native.c` | Trigger volumes and their models. |
| `codol_glass_groups.c` | Breakable glass groups: the owned group store and named lookup. |
| `codol_physics.c`, `codol_physics_native.c` | The physics step for launched and dropped objects. |
| `codol_map_entities.c` | The map entity string the game parses into spawn entities. |
| `codol_collision_abi.c` | The seam where staged data meets the retained clip-map ABI. |
| `codol_zone_collision.c` | Finding and loading the collision/world pair from a zone. |

Browse every declaration in this area: [collision](/reference/areas/collision).
