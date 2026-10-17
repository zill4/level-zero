"""Creates Level Zero materials and the /Game/LevelZero/Maps/Level0 map.
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="<this file>"
Re-runnable: the master material and the map are rebuilt from scratch; instances are re-parented."""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_DIR = '/Game/LevelZero/Materials'
MAP_PATH = '/Game/LevelZero/Maps/Level0'
SEED = 20261021
FOG = (0.30, 0.26, 0.13)

# Unlit, self-lit "Backrooms" shading: cheap on standalone VR (no dynamic lights), and back faces never go black.
#  - walls: ambient + warm bounce rising toward the ceiling panels (local cube height, 0 at floor .. 1 at ceiling)
#  - floor/ceiling: flat levels; panels: Color * EmissiveStrength
#  - distance haze toward FogColor using view depth (stays LWC-free)
HLSL = r'''
float3 n = normalize(N);
float3 c;
if (EmissiveStrength > 0.0)
{
    c = Color.rgb * EmissiveStrength;
}
else
{
    float h = saturate(Loc.z / 100.0 + 0.5);
    float isWall = 1.0 - saturate(abs(n.z) * 2.0 - 0.5);
    float wallLight = (Ambient + Bounce * h * h) * (0.88 + 0.12 * abs(n.x));
    float flatLight = (n.z > 0.0) ? FloorLight : CeilLight;
    float light = lerp(flatLight, wallLight, isWall);
    c = Color.rgb * light * float3(1.0, 0.93, 0.74);
}
float f = saturate((Depth - FogStart) / max(FogEnd - FogStart, 1.0));
f = 1.0 - (1.0 - f) * (1.0 - f);
return lerp(c, FogColor.rgb, f);
'''


def log(msg):
    unreal.log('LZ: ' + msg)


def scalar(m, name, value, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', value)
    return e


def vector(m, name, rgb, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', unreal.LinearColor(*rgb, 1.0))
    return e


def build_master():
    name = 'M_LZ_Surface'
    path = f'{MAT_DIR}/{name}'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = TOOLS.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)

    cust = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -250, 0)
    cust.set_editor_property('code', HLSL)
    cust.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cust.set_editor_property('description', 'LZ_BackroomsLight')
    names = ['Color', 'N', 'Loc', 'Depth', 'EmissiveStrength', 'Ambient', 'Bounce', 'FloorLight', 'CeilLight', 'FogStart', 'FogEnd', 'FogColor']
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    cust.set_editor_property('inputs', inputs)

    src = {
        'Color': vector(m, 'Color', (1, 1, 1), 0),
        'N': MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -700, 120),
        'Depth': MEL.create_material_expression(m, unreal.MaterialExpressionPixelDepth, -700, 240),
        'EmissiveStrength': scalar(m, 'EmissiveStrength', 0.0, 360),
        'Ambient': scalar(m, 'Ambient', 0.38, 440),
        'Bounce': scalar(m, 'Bounce', 0.62, 520),
        'FloorLight': scalar(m, 'FloorLight', 0.62, 600),
        'CeilLight': scalar(m, 'CeilLight', 0.80, 680),
        'FogStart': scalar(m, 'FogStart', 500.0, 760),
        'FogEnd': scalar(m, 'FogEnd', 3400.0, 840),
        'FogColor': vector(m, 'FogColor', FOG, 920),
    }
    try:
        src['Loc'] = MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, -700, 1040)
    except Exception as ex:  # older API: walls fall back to mid-height brightness
        log(f'LocalPosition unavailable ({ex}); using constant')
        k = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -700, 1040)
        src['Loc'] = k
    for n in names:
        ok = MEL.connect_material_expressions(src[n], '', cust, n)
        if not ok:
            raise RuntimeError(f'LZ: could not connect custom input {n}')
    MEL.connect_material_property(cust, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log(f'created {path} ({MEL.get_num_material_expressions(m)} expressions)')
    return m


def instance(name, parent, color, emissive=0.0):
    path = f'{MAT_DIR}/{name}'
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = TOOLS.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        log(f'created {path}')
    MEL.set_material_instance_parent(mi, parent)
    MEL.clear_all_material_instance_parameters(mi)
    MEL.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(*color, 1.0))
    if emissive > 0:
        MEL.set_material_instance_scalar_parameter_value(mi, 'EmissiveStrength', emissive)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    return mi


surface = build_master()
mats = {
    'wall_material': instance('MI_LZ_Wallpaper', surface, (0.80, 0.70, 0.38)),
    'floor_material': instance('MI_LZ_Carpet', surface, (0.46, 0.39, 0.20)),
    'ceiling_material': instance('MI_LZ_CeilingTile', surface, (0.82, 0.79, 0.66)),
    'door_frame_material': instance('MI_LZ_DoorFrame', surface, (0.66, 0.56, 0.30)),
    'light_panel_material': instance('MI_LZ_LightPanel', surface, (1.0, 0.95, 0.78), 3.5),
    'dead_panel_material': instance('MI_LZ_DeadPanel', surface, (0.22, 0.21, 0.17), 1.0),
    'exit_material': instance('MI_LZ_Exit', surface, (0.25, 1.0, 0.4), 5.0),
    'fog_shell_material': instance('MI_LZ_FogShell', surface, FOG),
}
monster_body = instance('MI_LZ_Monster', surface, (0.03, 0.026, 0.02))
monster_eyes = instance('MI_LZ_MonsterEyes', surface, (0.85, 0.9, 1.0), 2.5)
for old in ('M_LZ_Greybox', 'M_LZ_Emissive'):  # step-1 masters, superseded by M_LZ_Surface
    if EAL.does_asset_exist(f'{MAT_DIR}/{old}'):
        EAL.delete_asset(f'{MAT_DIR}/{old}')
        log(f'deleted old {old}')

# ---- Map
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if EAL.does_asset_exist(MAP_PATH):
    # Level0 is the editor startup map, so it is loaded now: switch to a blank map before deleting it.
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if not EAL.delete_asset(MAP_PATH):
        raise RuntimeError('LZ: could not delete old ' + MAP_PATH)
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
gen.ensure_nav_mesh_bounds()

director = eas.spawn_actor_from_class(unreal.LevelZeroGameDirector, unreal.Vector(0, 0, 0))
director.set_actor_label('LevelZeroDirector')
director.set_editor_property('maze', gen)
director.set_editor_property('monster_body_material', monster_body)
director.set_editor_property('monster_eye_material', monster_eyes)

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
s.set_editor_property('vignette_intensity', 0.4)
ppv.set_editor_property('settings', s)

saved = les.save_current_level()
log(f'save_current_level -> {saved}')
log(gen.get_stats_string())
gen.export_debug_ascii(unreal.Paths.project_saved_dir() + 'LevelZero/maze_ascii.txt')
log('CREATE_LEVEL0_DONE')
