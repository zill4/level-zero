import unreal
m = unreal.load_asset('/Game/LevelZero/Materials/M_LZ_TripoUnlit')
for f in ('used_with_instanced_static_meshes', 'used_with_skeletal_mesh'):
    m.set_editor_property(f, True)
unreal.MaterialEditingLibrary.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('LZ_FIX: ism=%s skel=%s' % (m.get_editor_property('used_with_instanced_static_meshes'), m.get_editor_property('used_with_skeletal_mesh')))
