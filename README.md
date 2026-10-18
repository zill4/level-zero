# Level Zero

Liminal-maze VR horror for the **PICO VibeLab** program. Working title (avoid "Backrooms" in the store title).
You wake up in an endless maze of yellow-wallpapered rooms that buzz with fluorescent light. Something is in here with you, and it can hear you. Find the exit before it finds you.

> **Standalone project.** Shares no code, assets or tooling with Oomi, oomi-unreal or Warlok. No Warlok-generated assets.
> **Platform:** UE 5.8 is *not* on VibeLab's listed platforms (Spatial SDK / Unity). Justin is getting PICO's written OK before concept lock (Wed Oct 21, 2026).

Plan of record: Obsidian `Projects/PICO Developer Program/PICO Backrooms - V1 Plan.md`.

## Stack
- Unreal Engine **5.8.3** (`E:\Epic\UE_5.8`), C++ module `LevelZero` + Blueprints
- Base: UE 5.8 Virtual Reality template (XRFramework pawn: teleport, snap turn, grab, menu)
- XR: stock **OpenXR** + **PICO OpenXR 1.6.1** (Fab, installed in engine) + PICOController
- Android: arm64-v8a only, Vulkan only, Min SDK 29 / Target SDK 34, `com.crispcode.levelzero`
- Rendering: forward shading, mobile multi-view, MSAA, no Lumen / Nanite / VSM, distance fields off on Android
- Toolchain (verified on zill4): Android SDK platform 36, build-tools 36.0.0, CMake 3.22.1, NDK 28.2.13676358 (UE 5.8 accepts r27c-r29), JDK 21 (`%USERPROFILE%\.jdks\jbr-21.0.11`; Android Studio's JDK 25 breaks UE 5.8's Gradle 8.13)

## Layout
```
LevelZero.uproject
Source/LevelZero/          C++ (MazeGenerator, TileStreamer, MonsterBrain, NoiseEmitter go here)
Content/LevelZero/         Our content: Maps, Modules, Monster, Audio, UI
Content/XRFramework ...    UE VR template packs (VR pawn, menus) - ok to prune Weapons later
Config/                    Engine/Game/Input + Android overrides
Tools/package_android.ps1  Builds the PICO APK with the right JDK/NDK env (session-only)
Docs/ASSET_LICENSES.md     License log for every third-party asset (required for Content Acquisition)
```

## Build
```powershell
# Editor (C++)
& "E:\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat" LevelZeroEditor Win64 Development -Project="D:\Unreal Projects\LevelZero\LevelZero.uproject" -WaitMutex
# Open
& "E:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Unreal Projects\LevelZero\LevelZero.uproject"
# PICO APK (+ install to a connected headset with -Install)
powershell -File Tools\package_android.ps1 [-Install]
```

## V1 scope (due Wed Oct 21, 2026)
| # | Feature | Done when |
|---|---|---|
| 1 | Procedural Level 0 maze | Seeded ~30x30 grid (recursive backtracker + room merges + pillar fields) from 4-6 modules via HISM; start/exit >= N cells apart; generates in < 2 s on device |
| 2 | Atmosphere | Fluorescent hum bed, random light flicker (emissive + audio blip), sickly-yellow exponential height fog, carpet footsteps |
| 3 | One monster, simple chase AI | Wander -> Investigate (AIPerception Hearing: sprint loud / walk quiet) -> Chase (Sight within X m) -> Lose track (Y s) -> Wander. Catch = death screen + restart |
| 4 | Exit to win | Original-looking exit door; reach it -> "Escaped" + time |
| 5 | EN/ZH menu | Start / Settings (locomotion, turn, vignette, language) / Quit, EN + 简体中文 via UE Localization, default from system locale |
| 6 | VR basics | Smooth + teleport locomotion, snap turn (30/45), comfort vignette on by default, recenter |
| 7 | Build | APK runs on a PICO device at 72+ fps in the test maze; short capture for V1 delivery |

Not in V1: story, multiple levels, collectibles, monster variants, save system.

## Rules (IP + comfort)
- Generic yellow-wallpaper liminal maze only. Our own textures. Original monster (name, design, behavior).
- Nothing from Kane Pixels / A24 / Backrooms wikis (CC BY-SA). No wiki entity names or level numbering beyond "Level 0".
- Monster from **Tripo paid plan only** (free tier = non-commercial, Tripo keeps rights). Keep prompts + source art as provenance.
- Fab assets marked "Allows usage with AI: No" must never be fed to Tripo or any generator.
- Scares are audio-first; never take control of the camera. Seated mode + low-intensity toggle. 72 fps floor.

## TODO
- [ ] Get PICO's written OK for Unreal (and horror content / age rating) before Oct 21
- [x] `Level0` map (`/Game/LevelZero/Maps/Level0`) is the editor + game startup map
- [x] `ALevelZeroMazeGenerator` (C++): seeded 30x30 maze, merged rooms, pillar halls, doorways, ceiling panels, BFS start/exit, `FindPath` for AI
- [x] Tile streaming: modules beyond 7 cells (was 3) (Chebyshev) of the camera are collapsed; collision stays on an invisible ISM
- [ ] 6 greybox modules (wall 2 m / 4 m, pillar, doorway, ceiling tile w/ light, floor) - our own art, emissive panels with baked AO in texture
- [ ] Atmosphere: hum loop, flicker component, exponential height fog, footstep surface sounds (CC0 / recorded; log in Docs/ASSET_LICENSES.md)
- [x] Look pass: 2-cell-wide corridors, more loops/rooms, self-lit warm materials (no black back faces), distance haze + fog shell
- [x] Runtime nav mesh (Dynamic; rebuilt right after the maze generates); VR teleport projects onto it (self-test 100/100)
- [x] `ALevelZeroMonster` + `ALevelZeroMonsterAI`: placeholder silhouette; Wander / Investigate (noise) / Chase (sight) / Search -> give up after 4 s out of sight
- [x] Player noise from head movement (walk quiet 6 m, sprint loud 24 m, teleport landing 10 m) via `ALevelZeroGameDirector`
- [x] Caught + Escaped screens (world-space card that lazily follows the head), auto-restart
- [x] Swap the placeholder monster for the Tripo model (keep capsule 34 x 125)
- [ ] EN/ZH localization (Localization Dashboard, Noto Sans SC OFL font)
- [ ] Comfort: vignette default on, snap turn default, teleport option, recenter
- [x] Tripo monster: retopo (19.5k tris) + rig + run clip
- [ ] Monster idle/walk/lunge clips (the FBX only has a run take); LODs
- [ ] First APK on a real PICO headset; profile to a locked 72 fps
- [ ] Capture V1 delivery clip

## Step 1: maze generator (done 2026-10-17)
- `Source/LevelZero/LevelZeroMazeGenerator.*`: recursive-backtracker maze + rooms + pillar halls + 10% loops + doorways; double-BFS start/exit (seed 20261021: 75 steps apart); 7 render ISMs (6 draw calls + exit) streamed per cell, invisible collision ISM, exit trigger + `OnExitReached`.
- `Tools/create_level0.py` rebuilds materials + `Level0`; `Tools/verify_level0.py` is the headless check (`LZ_VERIFY_OK`).
- Captures (offscreen, no window): `UnrealEditor.exe LevelZero.uproject /Game/LevelZero/Maps/Level0 -game -RenderOffscreen -ResX=1280 -ResY=720 -nohmd -LZFlythrough=C:/tmp/fly [-LZFlyCells=16 -LZFlySpeed=200]` writes PNG frames along the start->exit path (encode with ffmpeg). `-LZShot=<png> [-LZTopDown]` writes a still.
- Android: see `Docs/EMULATORS.md` (PICO emulator yes / Android XR emulator no) and `Tools/package_android.ps1 -Variant Store|Emulator`.
## Step 2: look pass, nav mesh, monster, round flow (done 2026-10-17)
- **Openness:** `bWideCorridors` carves the maze on a 15x15 macro grid and expands each macro cell to 2x2 (8 m corridors), with 30% internal wall stubs, 25% loops, 10 rooms (3-6 x 3-5) and 4 pillar halls. Seed 20261021: 384 wall segments (step 1: 767), 85% of interior boundaries open, start/exit 59 BFS steps apart.
- **Look:** a single unlit master `M_LZ_Surface` (Custom HLSL node): ambient + warm bounce rising toward the ceiling panels on walls, flat floor/ceiling levels, emissive panels, distance haze (500-3400 cm) toward a dim yellow. Every face is lit the same way, so nothing goes black. No dynamic lights (cheap on standalone VR). A two-sided fog shell around the maze, plus streaming at 7 cells, hides the edge of the streamed tiles.
- **Nav:** `NavMeshBoundsVolume` covering the maze (made by `EnsureNavMeshBounds()` from the create script), `RecastNavMesh RuntimeGeneration=Dynamic`, `BuildRuntimeNav()` (blocking `UNavigationSystemV1::Build`) right after the BeginPlay generate: 0.04-0.10 s on the 3900X. The director logs a self-test: teleport projection onto the nav mesh at 100/100 sample cells, plus a start->exit nav path.
- **Monster** (`LevelZeroMonster.*`, `LevelZeroMonsterAI.*`): 2.6 m primitive silhouette (capsule r34 / hh125, matches the default nav agent), pale eyes. Speeds 140 wander / 220 investigate / 330 chase. Sight: 20 m, 70 degree half-cone (or within 3.5 m), line trace at about 1.65 m. Loses the player after 4 s out of sight, searches the last known spot, then wanders. Falls back to the maze cell graph if a nav move fails.
- **Round flow** (`LevelZeroGameDirector.*`): spawns the monster at least 14 BFS cells from the start, turns head speed into noise, catch within 95 cm, exit within 0.45 cell; caught (red) / escaped (green, with time) card 1.2 m ahead; restarts after 5 s / 8 s.
- **Captures:** `-LZFlythrough=<dir> -LZFlyCells=40 -LZFlySpeed=150 -LZFlyFrames=900 -LZMonsterCell=14` puts the monster on the route (it holds position until it spots the camera). `-LZNoMonster` tests the exit/win path. The round starts on the first recorded frame.

## Step 3a: Tripo monster + props (2026-10-17)
- **Pipeline:** `Tools/import_tripo.py` (UE Python; runs Blender headless):
  - **Static props:** `GLB → Tools/tripo_reduce.py` (join, weld, decimate to a tri budget, scale, bottom-centre pivot). It keeps only the base colour (resized to 1k/2k), then imports into `/Game/LevelZero/Props` with `SM_`, `T_*_BaseColor` and `MI_` (parent `M_LZ_TripoUnlit`). No Nanite; box collision.
  - `card: wall|ceiling` bakes flush-mounted props (vent, light panel) to a single textured quad. `blur` softens label pseudo-text.
  - **Rigged monster:** `"type": "rig"`, FBX → `Tools/tripo_rig_prep.py`:
    - Splits the take(s) into clips by hips speed.
    - Removes root motion (linear detrend of the hips' travel, which keeps the bob and sway) and normalises the armature scale.
    - Exports GLB, then UE (Interchange) imports into `/Game/LevelZero/Monster/Rig/LZ_MonsterRig/` (`LZ_MonsterRig`, `_Skeleton`, `_PhysicsAsset`, `_Anim` = run clip, root motion off). `MI_LZ_MonsterRig` sits in `/Game/LevelZero/Monster/`. Rig yaw is -90 (Interchange imports Tripo meshes facing +Y).
  - Manifests: `Tools/tripo_monster.json` (rig, `--assign-monster`), `Tools/tripo_monster_static.json` (fallback) and `Tools/tripo_props.json`. Use `--only A,B` to re-import some assets.
  - Originals stay in `SourceArt/Tripo/` (LFS, not packaged). Reduced GLBs, stats JSON and previews go to `SourceArt/Tripo/Reduced/`.
  ```
  UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript "-script=Tools/import_tripo.py --manifest Tools/tripo_monster.json --assign-monster"
  UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript "-script=Tools/import_tripo.py --manifest Tools/tripo_props.json"
  ```
- **Monster:** `ALevelZeroMonster` picks the rigged mesh, then the static mesh, then the primitive placeholder. The capsule (34/125) and nav are unchanged. The mesh is scaled to `TargetHeight` = 260 cm, with its feet at the capsule bottom.
  - **Animation:** single-node playback driven by velocity. It walks while wandering or investigating and runs while chasing. The play rate is matched to speed (clip speed in body heights per second) times `WalkPlayRate` 0.8 / `RunPlayRate` 1.1, clamped to at least `MinPlayRate`. The FBX only has a run cycle, so the walk is the run slowed down; a `WalkAnim` can be dropped in later.
  - **Eyes:** pale glowing eyes ride the `Head` bone.
  - **Lintels:** a procedural duck under door lintels (door lintels are pawn-transparent `LintelCollisionISM`, so the 2.5 m capsule fits 2.3 m doors). Procedural bob, sway and lean only apply to the static fallback.
  - The director carries the wiring (`MonsterSkeletalMesh`, `MonsterRunAnim`, `MonsterWalkAnim`, speeds, eye height, yaw). Swapping the model means re-running the import.
- **Props:** `ALevelZeroMazeGenerator` places Tripo props with their own seeded stream (layout unchanged, deterministic per seed, checked in verify).
  - **Ceiling lights:** one light fixture per cell on the 4 m grid (replacing the emissive boxes; about 8% dead panels).
  - **Vents:** about 5% of closed walls.
  - **Clutter:** 16 clusters in rooms and dead ends (wall pieces against walls; chairs and wet-floor signs at least 90 cm off the cell centre).
  - **Landmarks:** 1–2 vending machines and arcade cabinets, at BFS ≥ 10 from the start.
  - **Instancing:** one ISM per mesh, with render instances streamed with the tiles. Collision lives in hidden non-streamed ISMs, so the nav mesh routes around props and streaming never dirties it. Start, exit and nearby cells stay clear.
- **Known limits (step 3a):**
  - No LODs. `set_lods` returns -1 and `StaticMeshEditorSubsystem` is unavailable in the `-run=pythonscript` commandlet, so props and monster are single-LOD. Tri budgets are already low (props 2-12k, monster 19.5k). Generate LODs from the editor UI later if profiling asks for it.
  - The monster's A-pose arm span may clip door posts.
  - Clutter is sparse (16 clusters / 23 items) by design. Raise `NumClutterClusters` for more.
  - Not tested on a PICO device or the emulator yet (the emulator was busy with Oomi).
- **Captures:** `-LZPropShots=<dir> -LZNoMonster` writes `prop_00..11.png` (landmarks and the first clusters).

## Step 4: data-driven level themes + 7 greybox levels (2026-10-17)
- **Theme asset:** `ULevelZeroTheme` (`Source/LevelZero/LevelZeroTheme.h`, a `UPrimaryDataAsset`) holds everything a level varies:
  - **Dimensions and layout:** grid size, cell size, corridor width in cells, ceiling height (plus separate room and hall ceilings), partition height, wall thickness.
  - **Room and hall shapes:** rooms, pillar halls, loops, doorways, and the layout style (`Maze` or `Aisles` with cross-aisles).
  - **Look:** palette, alt-floor pattern (rooms / halls / checker / stripes), light tint and fog.
  - **Lights:** spacing, lit/dead chance and fixture scale.
  - **Props and landmarks:** prop sets, cluster counts, landmark rules (count per mesh, min distance from start, spacing, corridors allowed).
  - **Flow and extras:** next-level map, stream radius, balcony ring, seat tiers.
- **Generator:** `ALevelZeroMazeGenerator::Theme`. `Generate()` first copies the theme onto the generator (`ApplyTheme`), so there is still one code path. Palette and fog are applied as dynamic instances of the shared `MI_LZ_*` / Tripo materials, including the monster's (`GetThemedMaterial`). A map without a theme behaves as before.
- **Exit chain:** reaching the exit shows "ESCAPED", then `ALevelZeroGameDirector::AdvanceOrRestart` opens the theme's `NextLevel` (or restarts if it has none).
- **Source of truth:** `Tools/levels/<id>.json`.
  - `level0.json` is hand-written and reproduces the step 3 Level 0 exactly (same seed, layout, panel and prop counts).
  - `L1..L6.json` are generated from the designer spec `Docs/levels_spec.json` by `python Tools/levels_from_spec.py`. The spec's colours are sRGB hex, converted to linear. Greybox layout knobs live in that script.
- **Build and verify:**
  ```
  UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript "-script=<abs>/Tools/create_level.py --all"        (or --theme L3)
  UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript "-script=<abs>/Tools/verify_level.py --theme L3"
  ```
  - `create_level.py` writes `DA_LZTheme_<id>` to `/Game/LevelZero/Themes`, rebuilds each map in place and links the chain.
  - `create_level0.py` / `verify_level0.py` are wrappers for Level 0.
  - `M_LZ_Surface` gained a `LightTint` vector parameter. Its default is the old hard-coded warm tint, so Level 0 is unchanged.

| Level | Map | Greybox |
|---|---|---|
| level0 Floor Zero | `Maps/Level0` | unchanged (step 3) |
| L1 Bulk Aisles | `Maps/L1_BulkAisles` | `Aisles` layout: 5 m teal rack walls (90 cm deep), 1-cell aisles, 2-cell cross-aisles every 6 cells, 9 m dark ceiling, scaled high-bay cards every 8 m, box clusters along racks, vending machines, 6-cell stream radius |
| L2 Showroom of Homes | `Maps/L2_Showroom` | 3 m vignette partitions in a 12 m black void, 14 open-front rooms (3.6 m openings) with wood-tone floors, rose carpet paths, sparse warm panels, CRT-cart landmarks, filing cabinets / boxes / chairs as stand-in furniture |
| L3 Pool Rooms | `Maps/L3_PoolRooms` | tiled 1-cell corridors, 4 m ceiling, cool light; rooms are "pools" (opaque water-blue floor, deck level); 2 tile-pillar forests; wet-floor signs |
| L4 Checkerboard Mall | `Maps/L4_Mall` | 2-cell concourses, checker floor, 12 storefront rooms (3 m openings), 6x6 atrium with a 9 m ceiling and a greybox balcony ring at 4.5 m; vending + arcade landmarks |
| L5 Screens 7-13 | `Maps/L5_Cinema` | purple 2-cell halls (3.5 m), striped carpet, 7 auditoriums (5x7 cells, 9 m ceilings) with visual stepped seat tiers, arcade/vending in the halls |
| L6 Indoor Suburb | `Maps/L6_Suburb` | 8 m cells (20x20), 15 m ceiling, 6.5 m x 3 m "house row" blocks forming 5 m streets, lawn cul-de-sacs, fog 6-30 m and a 4-cell stream radius |

- **Not built yet (needs new Tripo props or walkable verticality):**
  - Mezzanine and stairs (L2), escalators and a walkable second floor (L4), raked walkable floors (L5).
  - Water depth and wading (L3), the sky-window corridor (L3).
  - Per-level props from the spec's `new_props_needed`, the Heart Diner hub and the ending.
- **Monster twists:** none implemented yet. Every level uses the baseline AI; the twists are listed in `Docs/levels_spec.json`.
- **Fog vs streaming:** spec fog end distances past `(StreamRadiusCells + 3)` cells are clamped:
  - L1: 60 → 36 m.
  - L3: 40 → 32 m.
  - L4: 50 → 32 m.
  - L6: 15–70 → 6–30 m (on purpose, for its long sightlines).
