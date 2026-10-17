"""Creates Level Zero greybox materials and the /Game/LevelZero/Maps/Level0 map.
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="<this file>"
Re-runnable: existing materials are reused; the map is rebuilt from scratch."""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_DIR = '/Game/LevelZero/Materials'
MAP_PATH = '/Game/LevelZero/Maps/Level0'
SEED = 20261021


def log(msg):
    unreal.log('LZ: ' + msg)


def master(name, unlit):
    path = f'{MAT_DIR}/{name}'
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    m = TOOLS.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    if unlit:
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        inten = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 250)
        inten.set_editor_property('parameter_name', 'Intensity')
        inten.set_editor_property('default_value', 1.0)
        mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 0)
        MEL.connect_material_expressions(color, '', mul, 'A')
        MEL.connect_material_expressions(inten, '', mul, 'B')
        MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        MEL.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 250)
        rough.set_editor_property('r', 0.92)
        MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log(f'created {path}')
    return m


def instance(name, parent, color, intensity=None):
    path = f'{MAT_DIR}/{name}'
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = TOOLS.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        log(f'created {path}')
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(*color, 1.0))
    if intensity is not None:
        MEL.set_material_instance_scalar_parameter_value(mi, 'Intensity', intensity)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    return mi


lit = master('M_LZ_Greybox', False)
emis = master('M_LZ_Emissive', True)
mats = {
    'wall_material': instance('MI_LZ_Wallpaper', lit, (0.78, 0.68, 0.36)),
    'floor_material': instance('MI_LZ_Carpet', lit, (0.40, 0.34, 0.17)),
    'ceiling_material': instance('MI_LZ_CeilingTile', lit, (0.80, 0.77, 0.64)),
    'door_frame_material': instance('MI_LZ_DoorFrame', lit, (0.60, 0.51, 0.27)),
    'light_panel_material': instance('MI_LZ_LightPanel', emis, (1.0, 0.95, 0.78), 4.0),
    'dead_panel_material': instance('MI_LZ_DeadPanel', emis, (0.22, 0.21, 0.17), 1.0),
    'exit_material': instance('MI_LZ_Exit', emis, (0.25, 1.0, 0.4), 6.0),
}

# ---- Map
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if EAL.does_asset_exist(MAP_PATH):
    EAL.delete_asset(MAP_PATH)
ok = les.new_level(MAP_PATH)
log(f'new_level -> {ok}')
if not ok:
    raise RuntimeError('LZ: could not create ' + MAP_PATH)

player_start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(200, 200, 100))
gen = eas.spawn_actor_from_class(unreal.LevelZeroMazeGenerator, unreal.Vector(0, 0, 0))
gen.set_actor_label('LevelZeroMaze')
gen.set_editor_property('seed', SEED)
gen.set_editor_property('player_start_actor', player_start)
for k, v in mats.items():
    gen.set_editor_property(k, v)
gen.generate()

# Dim, warm, shadowless key light; the emissive panels sell the fluorescent look.
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-65, yaw=35))
sun.set_actor_label('LZ_WarmFill')
lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
lc.set_mobility(unreal.ComponentMobility.MOVABLE)
lc.set_intensity(2.5)
lc.set_light_color(unreal.LinearColor(1.0, 0.86, 0.58, 1.0))
lc.set_cast_shadows(False)

fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog.set_actor_label('LZ_YellowFog')
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property('fog_density', 0.12)
fc.set_editor_property('fog_height_falloff', 0.001)
for prop in ('fog_inscattering_luminance', 'fog_inscattering_color'):
    try:
        fc.set_editor_property(prop, unreal.LinearColor(0.32, 0.27, 0.12, 1.0))
        break
    except Exception:
        pass
fc.set_editor_property('start_distance', 150.0)

ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
ppv.set_actor_label('LZ_PostProcess')
ppv.set_editor_property('unbound', True)
s = ppv.get_editor_property('settings')
s.set_editor_property('override_auto_exposure_method', True)
s.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property('override_auto_exposure_apply_physical_camera_exposure', True)
s.set_editor_property('auto_exposure_apply_physical_camera_exposure', False)
s.set_editor_property('override_auto_exposure_bias', True)
s.set_editor_property('auto_exposure_bias', 0.0)
s.set_editor_property('override_vignette_intensity', True)
s.set_editor_property('vignette_intensity', 0.5)
ppv.set_editor_property('settings', s)

saved = les.save_current_level()
log(f'save_current_level -> {saved}')
log(gen.get_stats_string())
gen.export_debug_ascii(unreal.Paths.project_saved_dir() + 'LevelZero/maze_ascii.txt')
log('CREATE_LEVEL0_DONE')
