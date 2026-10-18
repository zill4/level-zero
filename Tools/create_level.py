"""Builds Level Zero levels from theme JSON (Tools/levels/<id>.json):
  - shared materials (M_LZ_Surface master + base instances; master only rebuilt when missing or with --rebuild-master)
  - one ULevelZeroTheme data asset per level: /Game/LevelZero/Themes/DA_LZTheme_<id>
  - one map per level (theme "map"), with generator (Theme set), director, post process, nav bounds
  - exit chain: each theme's NextLevel = the map of the theme named in "next"
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript "-script=<abs>/Tools/create_level.py --all"
     ... "-script=<abs>/Tools/create_level.py --theme level0 --theme mall"   (ids or json paths)
"""
import json
import os
import sys
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_DIR = '/Game/LevelZero/Materials'
THEME_DIR = '/Game/LevelZero/Themes'
PROP_DIR = '/Game/LevelZero/Props'
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LEVELS_DIR = os.path.join(PROJECT, 'Tools', 'levels')
FOG = (0.30, 0.26, 0.13)

# Unlit, self-lit "Backrooms" shading: cheap on standalone VR (no dynamic lights), and back faces never go black.
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
    c = Color.rgb * light * LightTint.rgb;
}
float f = saturate((Depth - FogStart) / max(FogEnd - FogStart, 1.0));
f = 1.0 - (1.0 - f) * (1.0 - f);
return lerp(c, FogColor.rgb, f);
'''


def log(msg):
    unreal.log('LZ: ' + msg)


# ------------------------------------------------------------------ materials
def _scalar(m, name, value, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', value)
    return e


def _vector(m, name, rgb, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', unreal.LinearColor(*rgb, 1.0))
    return e


def build_master(rebuild):
    name = 'M_LZ_Surface'
    path = f'{MAT_DIR}/{name}'
    if EAL.does_asset_exist(path) and not rebuild:
        m = unreal.load_asset(path)
        if 'LightTint' in [str(n) for n in MEL.get_vector_parameter_names(m)]:
            return m
        log('M_LZ_Surface has no LightTint parameter: rebuilding the master')
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
    names = ['Color', 'N', 'Loc', 'Depth', 'EmissiveStrength', 'Ambient', 'Bounce', 'FloorLight', 'CeilLight', 'FogStart', 'FogEnd', 'FogColor', 'LightTint']
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    cust.set_editor_property('inputs', inputs)
    src = {
        'Color': _vector(m, 'Color', (1, 1, 1), 0),
        'N': MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -700, 120),
        'Depth': MEL.create_material_expression(m, unreal.MaterialExpressionPixelDepth, -700, 240),
        'EmissiveStrength': _scalar(m, 'EmissiveStrength', 0.0, 360),
        'Ambient': _scalar(m, 'Ambient', 0.38, 440),
        'Bounce': _scalar(m, 'Bounce', 0.62, 520),
        'FloorLight': _scalar(m, 'FloorLight', 0.62, 600),
        'CeilLight': _scalar(m, 'CeilLight', 0.80, 680),
        'FogStart': _scalar(m, 'FogStart', 500.0, 760),
        'FogEnd': _scalar(m, 'FogEnd', 3400.0, 840),
        'FogColor': _vector(m, 'FogColor', FOG, 920),
        'LightTint': _vector(m, 'LightTint', (1.0, 0.93, 0.74), 1160),
        'Loc': MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, -700, 1040),
    }
    for n in names:
        if not MEL.connect_material_expressions(src[n], '', cust, n):
            raise RuntimeError(f'LZ: could not connect custom input {n}')
    MEL.connect_material_property(cust, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log(f'created {path}')
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


def build_materials(rebuild_master):
    surface = build_master(rebuild_master)
    # Base instances keep Level 0's look; other levels recolour them at runtime from their theme (dynamic instances).
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
    monster = (instance('MI_LZ_Monster', surface, (0.03, 0.026, 0.02)), instance('MI_LZ_MonsterEyes', surface, (0.85, 0.9, 1.0), 2.5))
    for old in ('M_LZ_Greybox', 'M_LZ_Emissive'):
        if EAL.does_asset_exist(f'{MAT_DIR}/{old}'):
            EAL.delete_asset(f'{MAT_DIR}/{old}')
    return mats, monster


# ------------------------------------------------------------------ theme asset
MESH_KEYS = {'ceiling_light_mesh', 'wall_vent_mesh'}
MESH_LIST_KEYS = {'clutter_wall_meshes', 'clutter_free_meshes', 'landmark_meshes'}
COLOR_KEYS = {'wall_color', 'floor_color', 'ceiling_color', 'trim_color', 'pillar_color', 'light_color', 'fog_color', 'alt_floor_color', 'light_tint'}
INT_KEYS = {'seed', 'width', 'height', 'corridor_width_cells', 'num_rooms', 'num_pillar_halls', 'stream_radius_cells', 'light_spacing_cells',
            'num_clutter_clusters', 'landmark_min_start_distance', 'landmark_spacing', 'cross_aisle_every', 'cross_aisle_width'}
ENUM_KEYS = {'layout_style': 'LZLayoutStyle', 'alt_floor_pattern': 'LZFloorPattern'}
POINT_KEYS = {'room_size_min', 'room_size_max', 'pillar_hall_size_min', 'pillar_hall_size_max', 'cluster_wall_items', 'cluster_free_items', 'landmarks_per_mesh'}
BOOL_RENAME = {'clusters_in_corridors': 'clusters_in_corridors', 'landmarks_in_corridors': 'landmarks_in_corridors', 'place_props': 'place_props', 'apply_palette': 'apply_palette'}
META_KEYS = {'map', 'next', 'notes', 'source', 'id', 'level_id', 'display_name', 'spec', 'min_openness'}


def prop_mesh(name):
    if not name:
        return None
    path = f'{PROP_DIR}/SM_{name}' if not name.startswith('/') else name
    if not EAL.does_asset_exist(path):
        log(f'WARNING: prop {path} missing, skipped')
        return None
    return unreal.load_asset(path)


def load_theme_json(ref):
    path = ref if ref.endswith('.json') else os.path.join(LEVELS_DIR, ref + '.json')
    with open(path) as f:
        t = json.load(f)
    t.setdefault('level_id', os.path.splitext(os.path.basename(path))[0])
    return t


def make_theme_asset(t):
    lid = t['level_id']
    name = f'DA_LZTheme_{lid}'
    path = f'{THEME_DIR}/{name}'
    if EAL.does_asset_exist(path):
        da = unreal.load_asset(path)
    else:
        fac = unreal.DataAssetFactory()
        fac.set_editor_property('data_asset_class', unreal.LevelZeroTheme)
        da = TOOLS.create_asset(name, THEME_DIR, unreal.LevelZeroTheme, fac)
        log(f'created {path}')
    da.set_editor_property('level_id', lid)
    da.set_editor_property('display_name', t.get('display_name', lid))
    for k, v in t.items():
        if k in META_KEYS or k.startswith('_'):
            continue
        if k in MESH_KEYS:
            v = prop_mesh(v)
        elif k in MESH_LIST_KEYS:
            v = [m for m in map(prop_mesh, v) if m]
        elif k in COLOR_KEYS:
            v = unreal.LinearColor(*[float(x) for x in v][:3], float(v[3]) if len(v) > 3 else 1.0)
        elif k in POINT_KEYS:
            v = unreal.IntPoint(int(v[0]), int(v[1]))
        elif k in ENUM_KEYS:
            v = getattr(getattr(unreal, ENUM_KEYS[k]), str(v).upper())
        elif isinstance(v, bool):
            pass
        elif k in INT_KEYS:
            v = int(v)
        elif isinstance(v, (int, float)):
            v = float(v)
        try:
            da.set_editor_property(k, v)
        except Exception as ex:
            raise RuntimeError(f'LZ: theme {lid}: cannot set {k}={v!r}: {ex}')
    EAL.save_loaded_asset(da)
    return da


# ------------------------------------------------------------------ monster wiring
def wire_monster(director, monster_mats):
    director.set_editor_property('monster_body_material', monster_mats[0])
    director.set_editor_property('monster_eye_material', monster_mats[1])
    tools = os.path.join(PROJECT, 'Tools')
    rig_stats = os.path.join(PROJECT, 'SourceArt', 'Tripo', 'Reduced', 'LZ_MonsterRig.json')
    ua = json.load(open(rig_stats)).get('ue_assets') if os.path.exists(rig_stats) else None
    if ua and EAL.does_asset_exist(ua['skeletal_mesh']):
        e = json.load(open(os.path.join(tools, 'tripo_monster.json')))['assets'][0]
        st = json.load(open(rig_stats))
        sp = {c['name']: c['root_speed_per_height'] for c in st['clips']}
        director.set_editor_property('monster_skeletal_mesh', unreal.load_asset(ua['skeletal_mesh']))
        director.set_editor_property('monster_mesh_material', unreal.load_asset(ua['material']))
        for k in ('Run', 'Walk'):
            p = ua['clips'].get(k)
            director.set_editor_property(f'monster_{k.lower()}_anim', unreal.load_asset(p) if p else None)
            director.set_editor_property(f'monster_{k.lower()}_anim_speed', float(sp.get(k, 0.0)))
        director.set_editor_property('monster_mesh_yaw_offset', float(e.get('yaw', 0)))
        director.set_editor_property('monster_eye_height', float(e.get('eye_height', 235)))
        return 'rig'
    if EAL.does_asset_exist('/Game/LevelZero/Monster/SM_LZ_Monster'):
        e = json.load(open(os.path.join(tools, 'tripo_monster_static.json')))['assets'][0]
        director.set_editor_property('monster_mesh', unreal.load_asset('/Game/LevelZero/Monster/SM_LZ_Monster'))
        director.set_editor_property('monster_mesh_material', unreal.load_asset('/Game/LevelZero/Monster/MI_LZ_Monster'))
        director.set_editor_property('monster_mesh_yaw_offset', float(e.get('yaw', 0)))
        return 'static'
    return 'placeholder'


# ------------------------------------------------------------------ map
def build_map(t, theme, mats, monster_mats):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    map_path = t['map']
    if EAL.does_asset_exist(map_path):
        # Rebuild in place: delete+recreate of a (possibly loaded) map is unreliable in the commandlet.
        if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
            raise RuntimeError('LZ: could not load ' + map_path)
        old = [a for a in eas.get_all_level_actors() if not isinstance(a, (unreal.WorldSettings, unreal.Brush))]
        eas.destroy_actors(old)
        log(f'{t["level_id"]}: cleared {len(old)} actors from existing {map_path}')
    elif not les.new_level(map_path):
        raise RuntimeError('LZ: could not create ' + map_path)
    player_start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(200, 200, 100))
    gen = eas.spawn_actor_from_class(unreal.LevelZeroMazeGenerator, unreal.Vector(0, 0, 0))
    gen.set_actor_label('LevelZeroMaze')
    gen.set_editor_property('player_start_actor', player_start)
    for k, v in mats.items():
        gen.set_editor_property(k, v)
    gen.set_editor_property('theme', theme)
    gen.generate()
    gen.ensure_nav_mesh_bounds()
    director = eas.spawn_actor_from_class(unreal.LevelZeroGameDirector, unreal.Vector(0, 0, 0))
    director.set_actor_label('LevelZeroDirector')
    director.set_editor_property('maze', gen)
    kind = wire_monster(director, monster_mats)
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
    log(f'{t["level_id"]}: save_current_level -> {les.save_current_level()}; monster={kind}')
    log(f'{t["level_id"]}: ' + gen.get_stats_string())
    gen.export_debug_ascii(unreal.Paths.project_saved_dir() + f'LevelZero/maze_ascii_{t["level_id"]}.txt')
    # Fog vs streaming: tiles past the stream radius pop out, so the haze should be opaque before that.
    radius_cm = t.get('stream_radius_cells', 5) * t.get('cell_size', 400)
    if t.get('fog_end', 3400) > radius_cm + 1500:
        log(f'NOTE: {t["level_id"]}: fog_end {t.get("fog_end")} is well past the stream radius ({radius_cm} cm); tiles may pop')


def link_chain(themes):
    by_id = {t['level_id']: (t, da) for t, da in themes}
    for t, da in themes:
        nxt = t.get('next')
        if not nxt:
            continue
        if nxt not in by_id:
            try:
                target = load_theme_json(nxt)['map']
            except Exception:
                log(f'WARNING: {t["level_id"]}: next theme {nxt} not found')
                continue
        else:
            target = by_id[nxt][0]['map']
        if not EAL.does_asset_exist(target):
            log(f'WARNING: {t["level_id"]}: next map {target} does not exist yet')
            continue
        da.set_editor_property('next_level', unreal.load_asset(target))
        EAL.save_loaded_asset(da)
        log(f'chain: {t["level_id"]} -> {nxt} ({target})')


def main(argv):
    refs, rebuild = [], False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--theme':
            refs.append(argv[i + 1]); i += 1
        elif a == '--all':
            refs += sorted(f[:-5] for f in os.listdir(LEVELS_DIR) if f.endswith('.json'))
        elif a == '--rebuild-master':
            rebuild = True
        i += 1
    if not refs:
        refs = ['level0']
    mats, monster_mats = build_materials(rebuild)
    themes = []
    for r in refs:
        t = load_theme_json(r)
        da = make_theme_asset(t)
        build_map(t, da, mats, monster_mats)
        themes.append((t, da))
    link_chain(themes)
    # Level0 stays the startup map; reopen it so the editor isn't left on the last built level.
    if EAL.does_asset_exist('/Game/LevelZero/Maps/Level0'):
        unreal.EditorLoadingAndSavingUtils.load_map('/Game/LevelZero/Maps/Level0')
    log('CREATE_LEVELS_DONE ' + ','.join(t['level_id'] for t, _ in themes))


main(sys.argv[1:])
