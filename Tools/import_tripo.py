"""Level Zero Tripo import pipeline (UE editor Python).

Single asset:
  UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="Tools/import_tripo.py --glb SourceArt/Tripo/chair.glb
      --name LZ_OfficeChair [--dest /Game/LevelZero/Props] [--tris 8000] [--tex 2048] [--height 1.0 | --longest 1.2]
      [--collision box|none] [--lods 0.5,0.25] [--tint 1,1,1] [--desat 0] [--glow 0] [--no-reduce]"
Batch (the queued props):
  ... -script="Tools/import_tripo.py --manifest Tools/tripo_props.json"
Monster:
  ... -script="Tools/import_tripo.py --manifest Tools/tripo_monster.json --assign-monster"

Per asset:
 1. Blender (Tools/tripo_reduce.py) decimates to --tris, keeps only base colour resized to --tex, scales to --height
    (or --longest) metres, pivot at bottom centre -> SourceArt/Tripo/Reduced/<name>.glb  (skip with --no-reduce)
 2. Interchange glTF import into a scratch folder, then renamed into <dest>: SM_<name>, Textures/T_<name>_BaseColor,
    MI_<name> (child of the shared unlit master M_LZ_TripoUnlit, same haze as the level). Interchange materials are deleted.
 3. Static mesh: Nanite off (mobile), LODs (default 100% / 50% / 25%), optional simple box collision.
    Texture: max size --tex, World LOD group, sRGB.
SourceArt/ is outside Content/, so the original GLBs are never cooked or packaged."""
import json
import os
import shlex
import subprocess
import sys

import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
BLENDER = os.environ.get('BLENDER', r'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe')
MASTER = '/Game/LevelZero/Materials/M_LZ_TripoUnlit'
SCRATCH = '/Game/LevelZero/_TripoImport'
# Keep in sync with Tools/create_level0.py (M_LZ_Surface) so imported meshes sit in the same haze.
FOG = (0.30, 0.26, 0.13)
FOG_START, FOG_END = 500.0, 3400.0

HLSL = r'''
float3 n = normalize(N);
float h = saturate(Loc.z / max(HeightRef, 1.0));
float light = Ambient + Bounce * h * h + Up * saturate(n.z);
float3 c = Tex.rgb * Tint.rgb * light;
float lum = dot(c, float3(0.299, 0.587, 0.114));
c = lerp(c, lum.xxx, Desaturate) + Tex.rgb * Glow;
float f = saturate((Depth - FogStart) / max(FogEnd - FogStart, 1.0));
f = 1.0 - (1.0 - f) * (1.0 - f);
return lerp(c, FogColor.rgb, f);
'''


def log(msg):
    unreal.log('LZ_TRIPO: ' + msg)


def parse(argv):
    opts, i = {}, 0
    flags = {'--no-reduce', '--assign-monster'}
    while i < len(argv):
        a = argv[i]
        if a in flags:
            opts[a[2:].replace('-', '_')] = True
            i += 1
        elif a.startswith('--'):
            opts[a[2:].replace('-', '_')] = argv[i + 1]
            i += 2
        else:
            i += 1
    return opts


def abspath(p):
    return p if os.path.isabs(p) else os.path.normpath(os.path.join(PROJECT, p))


def floats(s, n):
    v = [float(x) for x in str(s).split(',')]
    return v if len(v) == n else v * n


# ------------------------------------------------------------------ shared master material
USAGE_FLAGS = ('used_with_instanced_static_meshes', 'used_with_skeletal_mesh')  # props are ISM instances, the monster is skinned


def ensure_usage(m):
    changed = [f for f in USAGE_FLAGS if not m.get_editor_property(f)]
    for f in changed:
        m.set_editor_property(f, True)
    if changed:
        MEL.recompile_material(m)
        EAL.save_loaded_asset(m)
        log(f'enabled {changed} on {MASTER}')
    return m


def ensure_master():
    if EAL.does_asset_exist(MASTER):
        return ensure_usage(unreal.load_asset(MASTER))
    folder, name = MASTER.rsplit('/', 1)
    m = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', False)
    cust = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -250, 0)
    cust.set_editor_property('code', HLSL)
    cust.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cust.set_editor_property('description', 'LZ_TripoUnlit')
    names = ['Tex', 'Tint', 'N', 'Loc', 'Depth', 'HeightRef', 'Ambient', 'Bounce', 'Up', 'Desaturate', 'Glow',
             'FogStart', 'FogEnd', 'FogColor']
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    cust.set_editor_property('inputs', inputs)

    def scalar(name, v, y):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', v)
        return e

    def vector(name, rgb, y):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', unreal.LinearColor(*rgb, 1.0))
        return e

    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -250)
    tex.set_editor_property('parameter_name', 'BaseTex')
    tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
    src = {
        'Tex': tex,
        'Tint': vector('Tint', (1, 1, 1), 0),
        'N': MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -700, 100),
        'Loc': MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, -700, 180),
        'Depth': MEL.create_material_expression(m, unreal.MaterialExpressionPixelDepth, -700, 260),
        'HeightRef': scalar('HeightRef', 200.0, 340),
        'Ambient': scalar('Ambient', 0.55, 420),
        'Bounce': scalar('Bounce', 0.45, 500),
        'Up': scalar('Up', 0.10, 580),
        'Desaturate': scalar('Desaturate', 0.0, 660),
        'Glow': scalar('Glow', 0.0, 740),
        'FogStart': scalar('FogStart', FOG_START, 820),
        'FogEnd': scalar('FogEnd', FOG_END, 900),
        'FogColor': vector('FogColor', FOG, 980),
    }
    for n in names:
        out = 'RGB' if n == 'Tex' else ''
        if not MEL.connect_material_expressions(src[n], out, cust, n):
            raise RuntimeError(f'LZ_TRIPO: could not connect {n}')
    MEL.connect_material_property(cust, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log(f'created {MASTER}')
    return ensure_usage(m)


# ------------------------------------------------------------------ steps
def reduce_glb(entry):
    src = abspath(entry['glb'])
    out_dir = os.path.join(PROJECT, 'SourceArt', 'Tripo', 'Reduced')
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, entry['name'] + '.glb')
    if entry.get('no_reduce'):
        return src, None
    cmd = [BLENDER, '-b', '--factory-startup', '-P', os.path.join(PROJECT, 'Tools', 'tripo_reduce.py'), '--',
           '--in', src, '--out', out, '--tris', str(entry.get('tris', 20000)), '--tex', str(entry.get('tex', 2048)),
           '--stats', out[:-4] + '.json', '--preview', out[:-4] + '_preview.png']
    if entry.get('height'):
        cmd += ['--height', str(entry['height'])]
    if entry.get('longest'):
        cmd += ['--longest', str(entry['longest'])]
    if entry.get('rot'):
        cmd += ['--rot', str(entry['rot'])]
    if entry.get('card'):
        cmd += ['--card', str(entry['card'])]
    if entry.get('blur'):
        cmd += ['--blur', str(entry['blur'])]
    params = {'tris': int(entry.get('tris', 20000)), 'tex': int(entry.get('tex', 2048)),
              'height': float(entry['height']) if entry.get('height') else None,
              'longest': float(entry['longest']) if entry.get('longest') else None, 'rot': entry.get('rot') or None,
              'card': entry.get('card') or None, 'blur': float(entry['blur']) if entry.get('blur') else None,
              'card_v': 2 if entry.get('card') else None}
    stats_path = out[:-4] + '.json'
    if os.path.exists(out) and os.path.exists(stats_path) and os.path.getmtime(out) > os.path.getmtime(src):
        with open(stats_path) as f:
            old = json.load(f)
        if old.get('params') == params:
            log(f'reusing {out} (same settings)')
            return out, old
    log('blender: ' + ' '.join(shlex.quote(c) for c in cmd[4:]))
    res = subprocess.run(cmd, capture_output=True, text=True)
    stats = None
    for line in res.stdout.splitlines():
        if line.startswith('LZ_TRIPO_STATS '):
            stats = json.loads(line[len('LZ_TRIPO_STATS '):])
    if res.returncode != 0 or not stats:
        raise RuntimeError('LZ_TRIPO: Blender reduction failed:\n' + res.stdout[-2000:] + res.stderr[-2000:])
    log(f"reduced {stats['tris_before']} -> {stats['tris_after']} tris, textures {stats['textures']}")
    return out, stats


def import_glb(path, name):
    scratch = f'{SCRATCH}/{name}'
    if EAL.does_directory_exist(scratch):
        EAL.delete_directory(scratch)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', scratch)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    TOOLS.import_asset_tasks([task])
    imported = list(task.get_editor_property('imported_object_paths') or [])
    if not imported:  # Interchange may not fill the list; fall back to listing the folder
        imported = list(EAL.list_assets(scratch, recursive=True, include_folder=False))
    assets = [unreal.load_asset(p) for p in imported]
    assets = [a for a in assets if a]
    log(f'imported {len(assets)} assets: ' + ', '.join(f'{a.get_class().get_name()}:{a.get_name()}' for a in assets))
    return scratch, assets


def asset_file(path):
    return os.path.join(PROJECT, 'Content', path[len('/Game/'):] + '.uasset')


def on_disk(path):
    return EAL.does_asset_exist(path) or os.path.exists(asset_file(path))


def drop(path):
    """Delete an asset even if the registry hasn't scanned it yet."""
    old = unreal.load_asset(path) if on_disk(path) else None
    ok = bool(old) and EAL.delete_loaded_asset(old)
    if not ok and os.path.exists(asset_file(path)) and not EAL.does_asset_exist(path):
        os.remove(asset_file(path)); ok = True
    return ok


def move(asset, dest_path):
    if on_disk(dest_path) and not drop(dest_path):
        # Still referenced (e.g. by Level0's director): point those references at the new asset, then replace it.
        old = unreal.load_asset(dest_path)
        if not EAL.consolidate_assets(asset, [old]):
            raise RuntimeError(f'LZ_TRIPO: could not replace {dest_path}')
        if EAL.does_asset_exist(dest_path):
            EAL.delete_asset(dest_path)
    folder = dest_path.rsplit('/', 1)[0]
    EAL.make_directory(folder)
    src = asset.get_path_name().split('.')[0]
    if not EAL.rename_asset(src, dest_path):
        raise RuntimeError(f'LZ_TRIPO: rename {src} -> {dest_path} failed')
    return unreal.load_asset(dest_path)


def process(entry):
    name = entry['name']
    dest = entry.get('dest', '/Game/LevelZero/Props').rstrip('/')
    glb, stats = reduce_glb(entry)
    scratch, assets = import_glb(glb, name)
    meshes = [a for a in assets if isinstance(a, unreal.StaticMesh)]
    textures = [a for a in assets if isinstance(a, unreal.Texture2D)]
    if len(meshes) != 1:
        raise RuntimeError(f'LZ_TRIPO: expected 1 static mesh for {name}, got {len(meshes)}')

    mesh = move(meshes[0], f'{dest}/SM_{name}')
    tex = None
    if textures:
        tex = move(textures[0], f'{dest}/Textures/T_{name}_BaseColor')
        tex.set_editor_property('max_texture_size', int(entry.get('tex', 2048)))
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        tex.set_editor_property('srgb', True)
        EAL.save_loaded_asset(tex)

    # Material instance on the shared unlit master.
    master = ensure_master()
    mi_path = f'{dest}/MI_{name}'
    mi = unreal.load_asset(mi_path) if EAL.does_asset_exist(mi_path) else TOOLS.create_asset(
        f'MI_{name}', dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    if tex:
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', tex)
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*floats(entry.get('tint', '1,1,1'), 3), 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Desaturate', float(entry.get('desat', 0.0)))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Glow', float(entry.get('glow', 0.0)))
    bounds = mesh.get_bounding_box()
    MEL.set_material_instance_scalar_parameter_value(mi, 'HeightRef', float(entry.get('height_ref', max(bounds.max.z, 1.0))))
    for k in ('ambient', 'bounce', 'up'):
        if k in entry:
            MEL.set_material_instance_scalar_parameter_value(mi, k.capitalize(), float(entry[k]))
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mi)

    # Mobile settings: no Nanite, LODs, collision.
    ns = mesh.get_editor_property('nanite_settings')
    ns.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', ns)
    sme = None
    try:
        sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    except Exception:
        pass
    if sme is None:  # not initialised in -run=pythonscript commandlets; the static library has the same calls
        sme = unreal.EditorStaticMeshLibrary
    lods = [float(x) for x in str(entry.get('lods', '0.5,0.25')).split(',') if x.strip()]
    if lods:
        settings = [unreal.EditorScriptingMeshReductionSettings(1.0, 1.0)]
        screen = [0.45, 0.2, 0.08]
        for i, pct in enumerate(lods):
            settings.append(unreal.EditorScriptingMeshReductionSettings(pct, screen[min(i, len(screen) - 1)]))
        opts = unreal.EditorScriptingMeshReductionOptions()
        opts.set_editor_property('auto_compute_lod_screen_size', False)
        opts.set_editor_property('reduction_settings', settings)
        sme.set_lods(mesh, opts)
    sme.remove_collisions(mesh)
    if entry.get('collision', 'box') == 'box':
        sme.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    EAL.save_loaded_asset(mesh)

    # Drop Interchange's own materials/leftovers.
    if EAL.does_directory_exist(scratch):
        EAL.delete_directory(scratch)
    tris = [mesh.get_num_triangles(l) for l in range(mesh.get_num_lods())]
    tsize = (tex.blueprint_get_size_x(), tex.blueprint_get_size_y()) if tex else None
    log(f'DONE {name}: {mesh.get_path_name()} LOD tris {tris}, texture {tsize} (max {entry.get("tex", 2048)}), '
        f'bounds {bounds.max - bounds.min}, MI {mi_path}')
    return {'name': name, 'mesh': mesh, 'mi': mi, 'tris': tris, 'texture': tsize, 'reduce_stats': stats}


# ------------------------------------------------------------------ rigged (skeletal) assets
def prep_rig(entry):
    src = abspath(entry['fbx'])
    out_dir = os.path.join(PROJECT, 'SourceArt', 'Tripo', 'Reduced')
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, entry['name'] + '.glb')
    stats_path = out[:-4] + '.json'
    params = {'tex': int(entry.get('tex', 2048))}
    if os.path.exists(out) and os.path.exists(stats_path) and os.path.getmtime(out) > max(os.path.getmtime(src), os.path.getmtime(os.path.join(PROJECT, 'Tools', 'tripo_rig_prep.py'))):
        with open(stats_path) as f:
            old = json.load(f)
        if old.get('params') == params:
            log(f'reusing {out} (same settings)')
            return out, old
    cmd = [BLENDER, '-b', '--factory-startup', '-P', os.path.join(PROJECT, 'Tools', 'tripo_rig_prep.py'), '--',
           '--in', src, '--out', out, '--tex', str(params['tex']), '--stats', stats_path, '--preview', out[:-4] + '_preview.png']
    log('blender: ' + ' '.join(shlex.quote(x) for x in cmd[4:]))
    res = subprocess.run(cmd, capture_output=True, text=True)
    stats = None
    for line in res.stdout.splitlines():
        if line.startswith('LZ_RIG_STATS '):
            stats = json.loads(line[len('LZ_RIG_STATS '):])
    if res.returncode != 0 or not stats:
        raise RuntimeError('LZ_TRIPO: Blender rig prep failed:\n' + res.stdout[-2000:] + res.stderr[-2000:])
    log(f"rig prep: {stats['tris']} tris, {stats['bones']} bones, clips {stats['clips']}")
    return out, stats


def purge_legacy_rig(name, dest):
    """Remove rig assets left by the old rename-based import (SK_/SKEL_/PHYS_/A_). Files only: nothing has loaded them yet."""
    gone = []
    for p in (f'{dest}/SK_{name}', f'{dest}/SKEL_{name}', f'{dest}/PHYS_{name}'):
        if os.path.exists(asset_file(p)):
            os.remove(asset_file(p)); gone.append(p)
    adir = os.path.dirname(asset_file(f'{dest}/Anims/x'))
    if os.path.isdir(adir):
        for f in os.listdir(adir):
            if f.startswith(f'A_{name}_') and f.endswith('.uasset'):
                os.remove(os.path.join(adir, f)); gone.append(f)
    if gone:
        log(f'purged legacy rig assets {gone}')


def process_rig(entry):
    """Rig assets are imported straight into <dest>/Rig with Interchange's names and replaced in place on re-import,
    so Level0's references survive (deleting/renaming loaded skeletal assets is unreliable in a commandlet)."""
    name, dest = entry['name'], entry.get('dest', '/Game/LevelZero/Monster')
    purge_legacy_rig(name, dest)
    glb, stats = prep_rig(entry)
    rig_dir = f'{dest}/Rig'
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', glb)
    task.set_editor_property('destination_path', rig_dir)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    TOOLS.import_asset_tasks([task])
    paths = [p.split('.')[0] for p in EAL.list_assets(rig_dir, recursive=True, include_folder=False)]
    assets = [a for a in (unreal.load_asset(p) for p in paths) if a]
    log(f'rig assets in {rig_dir}: ' + ', '.join(f'{a.get_class().get_name()}:{a.get_path_name().split(".")[0]}' for a in assets))
    by = lambda cls: [a for a in assets if isinstance(a, cls)]
    skms, anims, texs = by(unreal.SkeletalMesh), by(unreal.AnimSequence), by(unreal.Texture2D)
    if not skms:
        raise RuntimeError(f'LZ_TRIPO: {name}: no skeletal mesh imported')
    if not anims:
        raise RuntimeError(f'LZ_TRIPO: {name}: no animation imported')
    skm = skms[0]
    clips = {}
    for a in anims:
        n = a.get_name().lower()
        key = 'Walk' if 'walk' in n else 'Run' if 'run' in n else None
        if key is None:
            key = min(stats['clips'], key=lambda c_: abs(c_['seconds'] - a.get_play_length()))['name']
        a.set_editor_property('enable_root_motion', False)  # clips are already in place
        clips[key] = a
    tex = texs[0] if texs else None
    if tex:
        tex.set_editor_property('max_texture_size', int(entry.get('tex', 2048)))
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        tex.set_editor_property('srgb', True)
    master = ensure_master()
    if not master.get_editor_property('used_with_skeletal_mesh'):
        master.set_editor_property('used_with_skeletal_mesh', True)
        MEL.recompile_material(master)
        EAL.save_loaded_asset(master)
        log('enabled used_with_skeletal_mesh on ' + MASTER)
    mi_path = f'{dest}/MI_{name}'
    mi = unreal.load_asset(mi_path) if on_disk(mi_path) else \
        TOOLS.create_asset(f'MI_{name}', dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    if tex:
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', tex)
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*floats(entry.get('tint', '1,1,1'), 3), 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Desaturate', float(entry.get('desat', 0.0)))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Glow', float(entry.get('glow', 0.0)))
    b = skm.get_bounds()
    bounds = b.box_extent * 2
    top = b.origin.z + b.box_extent.z   # local-space top (the material lights by local height)
    MEL.set_material_instance_scalar_parameter_value(mi, 'HeightRef', float(entry.get('height_ref_native', top)))
    for k in ('ambient', 'bounce', 'up'):
        if k in entry:
            MEL.set_material_instance_scalar_parameter_value(mi, k.capitalize(), float(entry[k]))
    MEL.update_material_instance(mi)
    mats = skm.get_editor_property('materials')
    for sm in mats:
        sm.set_editor_property('material_interface', mi)
    skm.set_editor_property('materials', mats)
    for a in [skm, mi, tex] + list(clips.values()):
        if a:
            EAL.save_loaded_asset(a, only_if_is_dirty=False)
    EAL.save_directory(rig_dir, only_if_is_dirty=False, recursive=True)
    speeds = {c_['name']: c_['root_speed_per_height'] for c_ in stats['clips']}  # body heights / s
    # Record the UE paths next to the Blender stats (create_level0.py reads them).
    stats_path = os.path.join(PROJECT, 'SourceArt', 'Tripo', 'Reduced', name + '.json')
    stats['ue_assets'] = {'skeletal_mesh': skm.get_path_name().split('.')[0], 'material': mi_path,
                          'clips': {k: a.get_path_name().split('.')[0] for k, a in clips.items()}}
    with open(stats_path, 'w') as f:
        json.dump(stats, f, indent=2)
    info = {k: (round(a.get_play_length(), 3), a.get_editor_property('enable_root_motion')) for k, a in clips.items()}
    log(f'DONE {name}: {skm.get_path_name()}, clips {info} (length s, root motion), speeds {speeds} heights/s, '
        f'bounds {bounds}, MI {mi_path}')
    return {'name': name, 'rig': True, 'skm': skm, 'mi': mi, 'clips': clips, 'speeds': speeds, 'stats': stats}


def assign_monster(result, yaw, entry=None):
    world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/LevelZero/Maps/Level0')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    dirs = [a for a in actors if isinstance(a, unreal.LevelZeroGameDirector)]
    if len(dirs) != 1:
        raise RuntimeError('LZ_TRIPO: expected 1 LevelZeroGameDirector in Level0')
    if result.get('rig'):
        d = dirs[0]
        d.set_editor_property('monster_skeletal_mesh', result['skm'])
        d.set_editor_property('monster_run_anim', result['clips'].get('Run'))
        d.set_editor_property('monster_walk_anim', result['clips'].get('Walk'))
        d.set_editor_property('monster_run_anim_speed', float(result['speeds'].get('Run', 0.0)))
        d.set_editor_property('monster_walk_anim_speed', float(result['speeds'].get('Walk', 0.0)))
        d.set_editor_property('monster_mesh', None)  # static fallback stays on disk, unreferenced (not cooked)
        if entry and 'eye_height' in entry:
            d.set_editor_property('monster_eye_height', float(entry['eye_height']))
    else:
        dirs[0].set_editor_property('monster_mesh', result['mesh'])
    dirs[0].set_editor_property('monster_mesh_material', result['mi'])
    dirs[0].set_editor_property('monster_mesh_yaw_offset', float(yaw))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log(f'assigned {result["name"]} to the director (yaw {yaw})')


def main():
    opts = parse(sys.argv[1:])
    if 'manifest' in opts:
        with open(abspath(opts['manifest'])) as f:
            manifest = json.load(f)
        defaults = manifest.get('defaults', {})
        entries = [dict(defaults, **e) for e in manifest['assets']]
        if opts.get('only'):
            keep = set(opts['only'].split(','))
            entries = [e for e in entries if e['name'] in keep]
    else:
        entries = [opts]
    results, failed = [], []
    for e in entries:
        srcf = e.get('fbx') or e.get('glb')
        if not os.path.exists(abspath(srcf)):
            log(f"SKIP {e['name']}: {srcf} not found")
            continue
        try:
            results.append(process_rig(e) if e.get('type') == 'rig' else process(e))
        except Exception as ex:  # keep going for the rest of a batch
            unreal.log_error(f"LZ_TRIPO: FAILED {e.get('name')}: {ex}")
            failed.append(e.get('name'))
    if opts.get('assign_monster') and results:
        assign_monster(results[0], entries[0].get('yaw', 0), entries[0])
    if EAL.does_directory_exist(SCRATCH):
        EAL.delete_directory(SCRATCH)
    log(f'IMPORT_TRIPO_DONE ok={len(results)} failed={failed}')


main()
