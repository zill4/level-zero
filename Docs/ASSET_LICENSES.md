# Asset license log

Every third-party or generated asset that ships in Level Zero gets a row. Required for PICO's Content Acquisition terms.
**Allowed sources (Justin, 2026-10-17):** Justin's own Fab/Epic library (already downloaded to the Epic Launcher VaultCache on zill4) and his own Tripo account (paid plan only). **Not allowed:** Warlok, random third-party downloads.

## In use

| Asset / path | Source | License | Date | Receipt / provenance | Notes |
|---|---|---|---|---|---|
| Content/XRFramework, VRSpectator, XRMannequins, Weapons, LevelPrototyping | Unreal Engine 5.8 VR template | UE EULA (engine content, ok to ship in UE projects) | 2026-10-17 | Engine install | Template packs; prune unused (Weapons) before ship |
| /Engine/BasicShapes/Cube, BasicShapeMaterial | Unreal Engine 5.8 | UE EULA | 2026-10-17 | Engine content | Greybox maze modules (instanced) |
| Content/LevelZero/Materials/* (M_LZ_Surface + 10 instances: wallpaper, carpet, ceiling, door frame, light/dead panel, exit, fog shell, monster, monster eyes) | Made for Level Zero (Tools/create_level0.py) | Own work | 2026-10-17 | Git history | Step 2: self-lit unlit master replaces M_LZ_Greybox / M_LZ_Emissive (deleted) |
| /Engine/BasicShapes/Cylinder, Sphere | Unreal Engine 5.8 | UE EULA | 2026-10-17 | Engine content | Fallback monster silhouette (used only when no Tripo mesh is assigned) and the monster's glowing eye spheres |
| Content/LevelZero/Materials/M_LZ_TripoUnlit + MI_LZ_* | Made for Level Zero (Tools/import_tripo.py) | Own work | 2026-10-17 | Git history | Unlit base-colour master for Tripo meshes (fake bounce light, desaturate, glow, same fog as M_LZ_Surface) |
| Engine default TextRender font/material | Unreal Engine 5.8 | UE EULA | 2026-10-17 | Engine content | Caught / Escaped cards |

## Tripo (Justin's own account)

All generated in **Justin's own Tripo account (justin@crispcode.io), Max plan (paid), private generations**, on **2026-10-17**. Paid-plan output is owned by the account holder and licensed for commercial use. No Fab/Epic library content and no third-party images were used as input. Original files stay in `SourceArt/Tripo/` (Git LFS, outside `Content/`, so never packaged). Only the reduced assets under `Content/LevelZero/` ship. Every asset is processed by `Tools/import_tripo.py` (Blender: `Tools/tripo_reduce.py` / `Tools/tripo_rig_prep.py`), with stats and previews in `SourceArt/Tripo/Reduced/`.

| Asset (Content path) | Source file (SourceArt/Tripo/) | Prompt summary | Credits | Shipped as |
|---|---|---|---|---|
| Monster, rigged: /Game/LevelZero/Monster/Rig/LZ_MonsterRig/SkeletalMeshes/LZ_MonsterRig (+ _Skeleton, _PhysicsAsset, _Anim = run clip), its Textures/ and Materials/, plus /Game/LevelZero/Monster/MI_LZ_MonsterRig | Rigged/lz_monster_rigged/lz_monster_rigged.fbx (+ .fbm textures) | Tall, emaciated, pale humanoid creature: long limbs, hollow eyes. Tripo retopo + auto-rig (Mixamo skeleton) + run animation | 55 (generation); retopo/rig/anim credits: *Justin to fill in* | 19,527 tris, 65 bones, base colour 2048², in-place run clip |
| Monster, static fallback: /Game/LevelZero/Monster/SM_LZ_Monster, MI_LZ_Monster | lz_monster.glb (hi-poly), Rigged/lz_monster_lowpoly.glb (retopo) | same generation | (same) | 29,999 tris (from 1,981,291), 2048²; unreferenced while the rig is assigned, so not cooked |
| /Game/LevelZero/Props/SM_LZ_CeilingLight | Props/lz_ceiling_light.glb | Fluorescent office ceiling light panel (troffer) | 55 | Baked to one textured quad (2 tris), 1024² |
| /Game/LevelZero/Props/SM_LZ_WallVent | Props/lz_wall_vent.glb | Metal wall air vent grille | 55 | Baked to one textured quad (2 tris), 1024² |
| /Game/LevelZero/Props/SM_LZ_OfficeChair | Props/lz_office_chair.glb | Worn fabric office chair | 55 | 6,000 tris, 1024² |
| /Game/LevelZero/Props/SM_LZ_Boxes | Props/lz_boxes.glb | Stack of cardboard boxes | 55 | 3,000 tris, 1024² |
| /Game/LevelZero/Props/SM_LZ_WetFloorSign | Props/lz_wet_floor_sign.glb | Yellow wet-floor A-frame sign | 55 | 2,999 tris, 1024² |
| /Game/LevelZero/Props/SM_LZ_FilingCabinet | Props/lz_filing_cabinet.glb | Beige metal filing cabinet | 55 | 5,000 tris, 1024² |
| /Game/LevelZero/Props/SM_LZ_WaterCooler | Props/lz_water_cooler.glb | Office water cooler with bottle | 55 | 5,000 tris, 1024² |
| /Game/LevelZero/Props/SM_LZ_CRTCart | Props/lz_crt_cart.glb | CRT television on a rolling AV cart | 55 | 9,000 tris, 2048² |
| /Game/LevelZero/Props/SM_LZ_VendingMachine | Props/lz_vending_machine.glb | Snack vending machine | 55 | 12,000 tris, 1024² (blurred + desaturated so the pseudo-text on the labels doesn't read) |
| /Game/LevelZero/Props/SM_LZ_ArcadeCabinet | Props/lz_arcade_cabinet.glb | Upright arcade cabinet | 55 | 11,999 tris, 2048² |

Prompt summaries are descriptive. The exact prompts are in the Tripo account history.

## Available in Justin's library (VaultCache on zill4, not imported)
License for all Fab library items: **Fab Standard License** (use inside a shipped game; no standalone redistribution). Before **any** Tripo/AI use of a pack, check its Fab listing's "Allows usage with AI" flag: "No" means it must never be fed to a generator. The local cache doesn't record that flag.

| Fab library item (VaultCache folder) | Size | Possible Level Zero use |
|---|---|---|
| Arch Vis Polish Pack 2 (ArchVisPolishPack2410) | 98 MB | **Best fit**: interior/arch-vis materials to restyle walls, carpet, ceiling tiles |
| Wooden Furniture (WoodenFu074762e64c86V1) | 1.2 GB | Sparse office/room props (chairs, tables) for merged rooms |
| Wooden Props (WoodenPrd2e46865b6a7V1) | 553 MB | Same |
| Kitchen Props (KitchenPc843ca996a29V1) | 820 MB | Break-room set dressing |
| TS Dungeon Kit 1 (TSDungeobe416c441572V1) | 3.7 GB | Possible "deeper level" variant later; not Level 0 style |
| Stylized Fantasy Enemy NPC Bundle (StylizedEnemyNPC) | 1.5 GB | Monster candidates, but stylized/fantasy tone clashes with liminal horror |
| Modular Stylized Fantasy Ghosts (ModularS7ceefff455c7V1) | 80 MB | Placeholder monster candidate (stylized) |
| Stylized Fantasy Creatures Bundle 1 & 2, Elementals, Mushroom/Bear/Forest-Spirit Minions, Slug, Rabbit, Rat/Horse Mounts | 0.02-0.7 GB each | Not a fit (fantasy) |
| Stylized landscapes/villages/castle/graveyard/underworld/winter city/dungeon, Lowpoly Alchemist's House, Farm Crops/Tools, Chests, Crystals, Fantasy Girls/Outfits/Weapons, Modular Human Male/Female, Character Creator Adventurer | various | Not a fit |
| PICO OpenXR (plugin) | - | Already installed in the engine |

Recommendation: restyle the greybox with **Arch Vis Polish Pack 2** materials + a few **Wooden Furniture** pieces, and make the monster in **Tripo (paid plan)** from original concept art; the library has no fitting horror creature.
