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
- [x] Tile streaming: modules beyond 3 cells (Chebyshev) of the camera are collapsed; collision stays on an invisible ISM
- [ ] 6 greybox modules (wall 2 m / 4 m, pillar, doorway, ceiling tile w/ light, floor) - our own art, emissive panels with baked AO in texture
- [ ] Atmosphere: hum loop, flicker component, exponential height fog, footstep surface sounds (CC0 / recorded; log in Docs/ASSET_LICENSES.md)
- [ ] `ALZMonster` + `ALZMonsterAIController`: AIPerception (Hearing + Sight), Behavior Tree, grid A* or runtime NavMesh (`Navigation Mesh` set to Dynamic)
- [ ] `ULZNoiseEmitter` on the pawn: MakeNoise loudness from locomotion speed
- [ ] Death + Escaped screens (UMG in world space), restart flow
- [ ] EN/ZH localization (Localization Dashboard, Noto Sans SC OFL font)
- [ ] Comfort: vignette default on, snap turn default, teleport option, recenter
- [ ] Tripo concept -> monster mesh (< 15k tris), retopo, rig, idle/walk/run/lunge
- [ ] First APK on a real PICO headset; profile to a locked 72 fps
- [ ] Capture V1 delivery clip

## Step 1: maze generator (done 2026-10-17)
- `Source/LevelZero/LevelZeroMazeGenerator.*`: recursive-backtracker maze + rooms + pillar halls + 10% loops + doorways; double-BFS start/exit (seed 20261021: 75 steps apart); 7 render ISMs (6 draw calls + exit) streamed per cell, invisible collision ISM, exit trigger + `OnExitReached`.
- `Tools/create_level0.py` rebuilds materials + `Level0`; `Tools/verify_level0.py` is the headless check (`LZ_VERIFY_OK`).
- Captures (offscreen, no window): `UnrealEditor.exe LevelZero.uproject /Game/LevelZero/Maps/Level0 -game -RenderOffscreen -ResX=1280 -ResY=720 -nohmd -LZFlythrough=C:/tmp/fly [-LZFlyCells=16 -LZFlySpeed=200]` writes PNG frames along the start->exit path (encode with ffmpeg). `-LZShot=<png> [-LZTopDown]` writes a still.
- Android: see `Docs/EMULATORS.md` (PICO emulator yes / Android XR emulator no) and `Tools/package_android.ps1 -Variant Store|Emulator`.