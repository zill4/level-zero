import unreal
path = '/Game/LevelZero/Monster/SM_LZ_Monster'
mesh = unreal.load_asset(path)
L = lambda m: unreal.log('LZ_LOD: ' + str(m))
L(f'before lods={mesh.get_num_lods()} tris={[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]}')
sub = None
try:
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
except Exception as e:
    L(f'subsystem err {e}')
L(f'subsystem={sub}')
for nm in ('StaticMeshReductionSettings', 'StaticMeshReductionOptions'):
    L(f'{nm} exists={hasattr(unreal, nm)}')
S = getattr(unreal, 'StaticMeshReductionSettings', unreal.EditorScriptingMeshReductionSettings)
O = getattr(unreal, 'StaticMeshReductionOptions', unreal.EditorScriptingMeshReductionOptions)
s0 = S(); s0.set_editor_property('percent_triangles', 1.0); s0.set_editor_property('screen_size', 1.0)
s1 = S(); s1.set_editor_property('percent_triangles', 0.5); s1.set_editor_property('screen_size', 0.45)
s2 = S(); s2.set_editor_property('percent_triangles', 0.25); s2.set_editor_property('screen_size', 0.2)
o = O(); o.set_editor_property('auto_compute_lod_screen_size', False); o.set_editor_property('reduction_settings', [s0, s1, s2])
L(f'settings {[ (x.percent_triangles, x.screen_size) for x in o.reduction_settings]}')
for who in [sub, unreal.EditorStaticMeshLibrary]:
    if who is None: continue
    try:
        r = who.set_lods(mesh, o)
        L(f'{who} set_lods -> {r}; lods={mesh.get_num_lods()} tris={[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]}')
        if mesh.get_num_lods() > 1: break
    except Exception as e:
        L(f'{who} set_lods err {e}')
if mesh.get_num_lods() <= 1:
    try:
        n = mesh.get_editor_property('lod_group'); L(f'lod_group={n}')
        (sub or unreal.EditorStaticMeshLibrary).set_lod_group(mesh, 'LargeProp', True)
        L(f'after lod_group lods={mesh.get_num_lods()} tris={[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]}')
    except Exception as e:
        L(f'lod group err {e}')
L(f'num source models {mesh.get_num_source_models() if hasattr(mesh, "get_num_source_models") else "?"}')
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
L('PROBE_DONE')
