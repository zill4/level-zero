"""Headless check: loads Level0, regenerates, verifies determinism/start/exit, exports the ASCII map.
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="<this file>" """
import unreal

MAP_PATH = '/Game/LevelZero/Maps/Level0'
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
assert world, 'LZ_VERIFY_FAIL: map did not load'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
unreal.log('LZ: actors in Level0: ' + ', '.join(sorted({a.get_class().get_name() for a in actors})))
gens = [a for a in actors if isinstance(a, unreal.LevelZeroMazeGenerator)]
assert len(gens) == 1, f'LZ_VERIFY_FAIL: expected 1 generator, got {len(gens)}'
gen = gens[0]
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
assert dist >= (w + h) // 2, f'LZ_VERIFY_FAIL: start/exit too close ({dist})'
# Different seed gives a different layout
seed = gen.get_editor_property('seed')
gen.set_editor_property('seed', seed + 1); gen.generate(); a3 = gen.get_debug_ascii()
gen.set_editor_property('seed', seed); gen.generate()
assert a3 != a1, 'LZ_VERIFY_FAIL: seed has no effect'
gen.export_debug_ascii(unreal.Paths.project_saved_dir() + 'LevelZero/maze_ascii.txt')
unreal.log('LZ: ' + gen.get_stats_string())
unreal.log(f'LZ: path start->exit {len(path)} cells; PlayerStart at {got}')
unreal.log('LZ_VERIFY_OK')
