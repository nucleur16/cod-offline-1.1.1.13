---
slug: subsystem-vehicle
title: Vehicles
space: subsystems
summary: src/codol/vehicle — the slot bank scripts precache into, the conversion from a CoDOL VehicleDef to a retained simulation record, and the spawner.
position: 60
tags: [vehicle]
---

The smallest CoDOL area — six files — but a good example of the shape the rest
of the tree uses: a **bank of published slots** for the client, a **conversion**
from the CoDOL asset to the retained engine record, and a **query seam** for
everything the retained record cannot hold.

## Three identifiers, deliberately distinct

| Identifier | Range | Meaning |
|---|---|---|
| Vehicle **slot** | 1..31 | The wire identity published to clients. |
| `s_vehicleInfos` **index** | engine table | The retained simulation record. |
| Definition **name** | string | What a script names. |

Confusing them is the failure this area is arranged to prevent, which is why
there are explicit converters both ways:

```c
int CODOL_RegisterVehicleName(const char *name, int create);  /* name -> slot */
int CODOL_VehicleInfoIndex(int slot);                          /* slot -> record */
int CODOL_BindVehicleInfoIndex(int info_index);                /* record -> slot */
const char *CODOL_VehicleName(int slot);                       /* slot -> name */
```

`CODOL_RegisterVehicleName` **publishes the name to clients that already hold a
gamestate** but does not load the definition; loading is a separate, idempotent
step (`CODOL_LoadVehicleDefinition`), which also registers the definition's
turret weapon.

`CODOL_VehicleName` never returns a fallback definition's name. That matters
because the load step *can* fall back, and a client told the wrong name renders
the wrong vehicle.

`CODOL_BindVehicleInfoIndex` covers the case where a vehicle is simulated but
no script ever precached it: it registers and publishes the retained record's
name on demand.

## Spawn lookup fails loudly

```c
int CODOL_VehicleInfoForSpawn(const char *name);
```

This requires registration during script initialisation; later spawns reuse the
published slot. A failure **drops the level** rather than handing retained
spawn/model fallback code an invalid record index — a deliberate choice, because
the retained fallback path would spawn something, and diagnosing the wrong
vehicle later is far harder than diagnosing a refused map load.

## Conversion

```c
int CODOL_ConvertMountedVehicle(const codol_mounted_vehicle_t *source,
                                codol_vehicle_native_t *out);
int CODOL_LoadNativeVehicleDefinition(const char *name, codol_vehicle_native_t *out);
int CODOL_InstallNativeVehicleRecord(const char *name);
const codol_vehicle_native_t *CODOL_NativeVehicleDefinition(int info_index);
```

`ConvertMountedVehicle` is a pure conversion: it copies every string it keeps so
the result outlives the archive, performs no engine calls and registers nothing.
`LoadNativeVehicleDefinition` distinguishes three outcomes — `1` converted, `0`
no such asset, `-1` present but unusable.

`InstallNativeVehicleRecord` is the step with side effects: it installs the
converted definition as a retained `s_vehicleInfos` record, registers its turret
weapon and resolves its six retained sound aliases. An already-installed
definition returns its existing index.

`CODOL_NativeVehicleDefinition` is **the query seam for every attribute the
retained `vehicle_info_t` cannot hold** — health, physics preset and profile,
all 47 sounds, compass materials and the C1 extension block. If you need a
vehicle attribute that IW3 never had, this is where it lives.

## Spawners and spawned instances

```c
typedef struct { int known, vehicle_slot, vehicle_info; } codol_vehicle_spawner_t;

int CODOL_CreateVehicleSpawner(gentity_t *entity, const char *vehicle_name);
```

A spawner is a **server-only entity category**, separate from IW3's
`ET_EVENTS = 17`. Creating one loads the vehicle definition; no live vehicle
pool entry exists until something actually spawns.

When a spawn does succeed, `CODOL_CaptureSpawnedVehicle` records the real
instance/definition pair in the entity extension **before snapshots can copy the
entity**, so the published identity is the one that was actually spawned.
`CODOL_CaptureVehicleSpawnClient` records the owner: helicopter spawn has an
explicit player owner, ordinary vehicle spawn records zero, matching C1's
absent-owner initialisation.

Per-map lifecycle is `CODOL_ResetNativeVehicles`, which must be called wherever
`s_numVehicleInfos` is cleared (`G_VehiclesInit`) or vehicle slots are reset.

Browse every declaration in this area: [vehicle](/reference/areas/vehicle).
