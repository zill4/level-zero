"""Headless check (any level: --theme <id>): loads the map, verifies the maze (determinism, start/exit, wide layout), the director,
the nav bounds and the monster defaults, and exports the ASCII map.
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="<this file>" """
import json
import os
import sys
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
_ref = 'level0'
for _i, _a in enumerate(sys.argv):
    if _a == '--theme' and _i + 1 < len(sys.argv):
        _ref = sys.argv[_i + 1]
_tpath = _ref if _ref.endswith('.json') else os.path.join(PROJECT, 'Tools', 'levels', _ref + '.json')
TJ = json.load(open(_tpath))
LID = TJ.get('level_id', _ref)
MAP_PATH = TJ['map']
unreal.log(f'LZ: verifying {LID} ({MAP_PATH})')
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
assert world, 'LZ_VERIFY_FAIL: map did not load'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
unreal.log(f'LZ: actors in {LID}: ' + ', '.join(sorted({a.get_class().get_name() for a in actors})))
gens = [a for a in actors if isinstance(a, unreal.LevelZeroMazeGenerator)]
assert len(gens) == 1, f'LZ_VERIFY_FAIL: expected 1 generator, got {len(gens)}'
gen = gens[0]
theme = gen.get_editor_property('theme')
assert theme is not None and str(theme.get_editor_property('level_id')) == LID, f'LZ_VERIFY_FAIL: generator theme missing or not {LID}'
a1 = gen.get_debug_ascii()
gen.generate()
a2 = gen.get_debug_ascii()
assert a1 == a2 and len(a1) > 0, 'LZ_VERIFY_FAIL: layout not deterministic for a fixed seed'
start, exit_ = gen.get_editor_property('start_cell'), gen.get_editor_property('exit_cell')
dist = gen.get_editor_property('start_to_exit_distance')
path = gen.find_path(start, exit_)
assert len(path) == dist + 1, f'LZ_VERIFY_FAIL: path len {len(path)} vs bfs {dist}'
ps = gen.get_editor_property('player_start_actor')
assert ps is not None, 'LZ_VERIFY_FAIL: no PlayerStart linked'
want = gen.get_cell_center_world(start, 100.0)
got = ps.get_actor_location()
assert (want - got).length() < 1.0, 'LZ_VERIFY_FAIL: PlayerStart not on start cell'
w, h = gen.get_editor_property('width'), gen.get_editor_property('height')
assert dist >= (w + h) // 4, f'LZ_VERIFY_FAIL: start/exit too close ({dist})'
CW = int(TJ.get('corridor_width_cells', 2))
assert gen.get_editor_property('wide_corridors') == (CW >= 2), 'LZ_VERIFY_FAIL: corridor width not applied'
assert abs(gen.get_editor_property('wall_height') - float(TJ.get('ceiling_height', 300))) < 0.5, 'LZ_VERIFY_FAIL: ceiling height not applied'

# Openness: fraction of interior cell boundaries that are open (step 1 perfect-ish maze was ~0.55).
lines = a1.split('\n')
open_b = total_b = 0
for r, line in enumerate(lines):
    for c, ch in enumerate(line):
        interior = 0 < r < len(lines) - 1 and 0 < c < len(line) - 1
        if interior and ((r % 2 == 1 and c % 2 == 0) or (r % 2 == 0 and c % 2 == 1)):
            total_b += 1
            open_b += ch != '#'
openness = open_b / max(total_b, 1)
MIN_OPEN = float(TJ.get('min_openness', 0.7 if CW >= 2 else 0.45))
assert openness > MIN_OPEN, f'LZ_VERIFY_FAIL: maze not open enough ({openness:.2f} <= {MIN_OPEN})'

seed = gen.get_editor_property('seed')
theme.set_editor_property('seed', seed + 1); gen.generate(); a3 = gen.get_debug_ascii()  # the theme owns the seed
theme.set_editor_property('seed', seed); gen.generate()
assert a3 != a1, 'LZ_VERIFY_FAIL: seed has no effect'

# Navigation bounds cover the maze.
vols = [a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume)]
assert len(vols) == 1, f'LZ_VERIFY_FAIL: expected 1 NavMeshBoundsVolume, got {len(vols)}'
origin, extent = vols[0].get_actor_bounds(False)
cs = gen.get_editor_property('cell_size')
assert extent.x >= w * cs * 0.5 and extent.y >= h * cs * 0.5 and extent.z > 100, f'LZ_VERIFY_FAIL: nav bounds too small {extent}'

# Director + monster defaults.
dirs = [a for a in actors if isinstance(a, unreal.LevelZeroGameDirector)]
assert len(dirs) == 1, f'LZ_VERIFY_FAIL: expected 1 director, got {len(dirs)}'
assert dirs[0].get_editor_property('maze') == gen, 'LZ_VERIFY_FAIL: director not linked to maze'
assert dirs[0].get_editor_property('monster_body_material') is not None, 'LZ_VERIFY_FAIL: monster material missing'
mcdo = unreal.get_default_object(unreal.LevelZeroMonster)
ws, cs_, chs = (mcdo.get_editor_property(p) for p in ('wander_speed', 'investigate_speed', 'chase_speed'))
assert ws < cs_ < chs, 'LZ_VERIFY_FAIL: monster speeds not ordered'
cap = mcdo.get_editor_property('capsule_component')
assert abs(cap.get_unscaled_capsule_radius() - 34.0) < 0.1 and abs(cap.get_unscaled_capsule_half_height() - 125.0) < 0.1, 'LZ_VERIFY_FAIL: monster capsule changed'

# Tripo monster mesh: mobile budget.
mesh = dirs[0].get_editor_property('monster_mesh')
mesh_info = 'placeholder'
if mesh:
    lod_tris = [mesh.get_num_triangles(l) for l in range(mesh.get_num_lods())]
    assert lod_tris[0] <= 40000, f'LZ_VERIFY_FAIL: monster LOD0 has {lod_tris[0]} tris (> 40k)'
    assert not mesh.get_editor_property('nanite_settings').enabled, 'LZ_VERIFY_FAIL: monster mesh has Nanite on'
    mi = dirs[0].get_editor_property('monster_mesh_material')
    assert mi is not None, 'LZ_VERIFY_FAIL: monster mesh material missing'
    tex = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi, 'BaseTex')
    assert tex is not None and tex.get_editor_property('max_texture_size') in (1024, 2048), 'LZ_VERIFY_FAIL: monster texture not capped to 1k/2k'
    size = mesh.get_bounding_box().max - mesh.get_bounding_box().min
    mesh_info = f'{mesh.get_name()} LOD tris {lod_tris}, size {size.x:.0f}x{size.y:.0f}x{size.z:.0f} cm, tex {tex.get_name()} max {tex.get_editor_property("max_texture_size")}'

# Tripo props: budgets, no Nanite, deterministic placement.
BUDGET = {'LZ_CeilingLight': 1000, 'LZ_WallVent': 1000, 'LZ_VendingMachine': 15000, 'LZ_ArcadeCabinet': 15000, 'LZ_CRTCart': 15000}
props = [gen.get_editor_property('ceiling_light_mesh'), gen.get_editor_property('wall_vent_mesh')] + list(gen.get_editor_property('clutter_wall_meshes')) + list(gen.get_editor_property('clutter_free_meshes')) + list(gen.get_editor_property('landmark_meshes'))
props = [m for m in props if m]
prop_info = []
for m in props:
    n = m.get_name()[3:]
    t0 = m.get_num_triangles(0)
    assert t0 <= BUDGET.get(n, 8000), f'LZ_VERIFY_FAIL: {n} LOD0 has {t0} tris (budget {BUDGET.get(n, 8000)})'
    assert not m.get_editor_property('nanite_settings').enabled, f'LZ_VERIFY_FAIL: {n} has Nanite on'
    prop_info.append(f'{n}={t0}')
if props:
    s1 = gen.get_prop_summary(); gen.generate(); s2 = gen.get_prop_summary()
    assert s1 == s2, f'LZ_VERIFY_FAIL: prop placement not deterministic ({s1} vs {s2})'
    assert gen.get_num_prop_instances() > 0, 'LZ_VERIFY_FAIL: prop meshes assigned but nothing placed'

# Rigged monster: budget + clips in place.
skm = dirs[0].get_editor_property('monster_skeletal_mesh')
if skm:
    run = dirs[0].get_editor_property('monster_run_anim')
    walk = dirs[0].get_editor_property('monster_walk_anim')
    assert run or walk, 'LZ_VERIFY_FAIL: rigged monster has no clips'
    for a in [x for x in (run, walk) if x]:
        assert not a.get_editor_property('enable_root_motion'), f'LZ_VERIFY_FAIL: {a.get_name()} has root motion on'
        s_a, s_m = a.get_editor_property('skeleton'), skm.get_editor_property('skeleton')
        assert s_a and s_m and s_a.get_path_name() == s_m.get_path_name(), f'LZ_VERIFY_FAIL: {a.get_name()} skeleton mismatch ({s_a.get_path_name() if s_a else None} vs {s_m.get_path_name() if s_m else None})'
    mi = dirs[0].get_editor_property('monster_mesh_material')
    assert mi is not None, 'LZ_VERIFY_FAIL: rig material missing'
    tex = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi, 'BaseTex')
    assert tex is not None and tex.get_editor_property('max_texture_size') in (1024, 2048), 'LZ_VERIFY_FAIL: rig texture not capped to 1k/2k'
    mesh_info = (f'{skm.get_name()} (rig) clips run={run.get_name() if run else None} {run.get_play_length() if run else 0:.2f}s '
                 f'walk={walk.get_name() if walk else "(run slowed)"}; speeds run={dirs[0].get_editor_property("monster_run_anim_speed"):.2f} heights/s; bounds {skm.get_bounds().box_extent * 2}; '
                 f'tex {tex.get_name()} max {tex.get_editor_property("max_texture_size")}')

# Exit chain.
nxt_info = 'restart'
if TJ.get('next'):
    nl = theme.get_editor_property('next_level')
    nl_path = str(nl.get_path_name()) if hasattr(nl, 'get_path_name') and nl else str(nl or '')
    assert nl_path and 'None' not in nl_path, f'LZ_VERIFY_FAIL: {LID} has next={TJ["next"]} but no NextLevel'
    assert unreal.EditorAssetLibrary.does_asset_exist(nl_path.split('.')[0]), f'LZ_VERIFY_FAIL: next level {nl_path} missing'
    nxt_info = nl_path.split('.')[0]

# Navigation reachability (cell graph above; nav mesh here if the editor nav system can build headless).
nav_info = 'deferred to runtime NavSelfTest'
try:
    if gen.build_runtime_nav():
        sp = gen.get_cell_center_world(start, 50.0)
        ep = gen.get_cell_center_world(exit_, 50.0)
        npath = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, sp, ep)
        pts = list(npath.get_editor_property('path_points')) if npath else []
        partial = npath.is_partial() if npath else True
        nav_info = f'nav path {len(pts)} points, partial={partial}'
        if pts:
            assert not partial and (pts[-1] - ep).length() < cs, f'LZ_VERIFY_FAIL: nav path start->exit incomplete ({nav_info})'
        else:
            nav_info += ' (no nav data in commandlet; deferred to runtime NavSelfTest)'
except AssertionError:
    raise
except Exception as ex:
    nav_info = f'nav check skipped ({ex}); deferred to runtime NavSelfTest'

aicdo = unreal.get_default_object(unreal.LevelZeroMonsterAI)
assert aicdo.get_editor_property('lose_sight_seconds') > 1.0, 'LZ_VERIFY_FAIL: lose-sight time'

gen.export_debug_ascii(unreal.Paths.project_saved_dir() + f'LevelZero/maze_ascii_{LID}.txt')
unreal.log('LZ: ' + gen.get_stats_string())
unreal.log(f'LZ: monster mesh: {mesh_info}')
unreal.log(f'LZ: {gen.get_prop_summary()}; prop LOD0 tris: {", ".join(prop_info) or "none"}')
unreal.log(f'LZ: openness {openness:.2f}; nav bounds extent {extent}; monster speeds {ws}/{cs_}/{chs}')
unreal.log(f'LZ: path start->exit {len(path)} cells; PlayerStart at {got}')
unreal.log(f'LZ: {LID}: next={nxt_info}; {nav_info}')
unreal.log(f'LZ_VERIFY_OK {LID}')
