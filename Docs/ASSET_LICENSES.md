# Asset license log

Every third-party or generated asset that ships in Level Zero gets a row. Required for PICO's Content Acquisition terms.
**Allowed sources (Justin, 2026-10-17):** Justin's own Fab/Epic library (already downloaded to the Epic Launcher VaultCache on zill4) and his own Tripo account (paid plan only). **Not allowed:** Warlok, random third-party downloads.

## In use

| Asset / path | Source | License | Date | Receipt / provenance | Notes |
|---|---|---|---|---|---|
| Content/XRFramework, VRSpectator, XRMannequins, Weapons, LevelPrototyping | Unreal Engine 5.8 VR template | UE EULA (engine content, ok to ship in UE projects) | 2026-10-17 | Engine install | Template packs; prune unused (Weapons) before ship |
| /Engine/BasicShapes/Cube, BasicShapeMaterial | Unreal Engine 5.8 | UE EULA | 2026-10-17 | Engine content | Greybox maze modules (instanced) |
| Content/LevelZero/Materials/* (M_LZ_Surface + 10 instances: wallpaper, carpet, ceiling, door frame, light/dead panel, exit, fog shell, monster, monster eyes) | Made for Level Zero (Tools/create_level0.py) | Own work | 2026-10-17 | Git history | Step 2: self-lit unlit master replaces M_LZ_Greybox / M_LZ_Emissive (deleted) |
| /Engine/BasicShapes/Cylinder, Sphere | Unreal Engine 5.8 | UE EULA | 2026-10-17 | Engine content | Placeholder monster silhouette (until the Tripo model) |
| Engine default TextRender font/material | Unreal Engine 5.8 | UE EULA | 2026-10-17 | Engine content | Caught / Escaped cards |

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
