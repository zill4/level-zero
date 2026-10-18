"""Level Zero - prepare a rigged Tripo/Mixamo FBX for UE (Blender, headless).

  blender -b --factory-startup -P Tools/tripo_rig_prep.py -- --in X.fbx --out Y.glb [--stats Y.json] [--preview Y_preview.png] [--tex 2048]

* Finds the moving sections of every take by the hips' ground speed (a take that holds a pose after the
  motion, or two clips joined in one take, is split by timing). One section -> "Run" (or "Walk" if slow);
  two -> the slower is "Walk", the faster "Run".
* Makes each clip in-place: removes the hips' net horizontal travel (linear detrend), keeps the bob and sway.
  The removed speed is reported (m/s, native scale) so the game can match play rate to velocity.
* Keeps only the base colour texture (unlit game material), resized to --tex. Exports GLB (skin + clips).
"""
import bpy, sys, os, json, math, time
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default
SRC, DST, STATS, PREVIEW = arg('--in'), arg('--out'), arg('--stats'), arg('--preview')
TEX = int(arg('--tex', 2048))
t0 = time.time()

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
sc = bpy.context.scene
fps = sc.render.fps / sc.render.fps_base
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
meshes = [o for o in sc.objects if o.type == 'MESH']
hips = next(b for b in arm.pose.bones if b.name.lower().endswith('hips'))
src_actions = list(bpy.data.actions)
tris = 0
for m in meshes:
    m.data.calc_loop_triangles(); tris += len(m.data.loop_triangles)
pts = [m.matrix_world @ v.co for m in meshes for v in m.data.vertices]
height = max(p.z for p in pts) - min(p.z for p in pts)

def hips_world(f):
    sc.frame_set(f)
    return (arm.matrix_world @ hips.head).copy()

# ---- find moving sections
sections = []
for act in src_actions:
    arm.animation_data_create(); arm.animation_data.action = act
    f0, f1 = int(round(act.frame_range[0])), int(round(act.frame_range[1]))
    P = {f: hips_world(f) for f in range(f0, f1 + 1)}
    v = {f: ((P[f] - P[f - 1]).to_2d().length * fps if f > f0 else 0.0) for f in P}
    vmax = max(v.values()) or 1.0
    moving = [f for f in P if v[f] > 0.25 * vmax]
    runs, cur = [], []
    for f in moving:
        if cur and f != cur[-1] + 1:
            runs.append(cur); cur = []
        cur.append(f)
    if cur: runs.append(cur)
    for r in runs:
        a, b = r[0] - 1, r[-1]          # include the frame before motion starts
        if b - a < 8: continue
        d = (P[b] - P[a]); d.z = 0
        sections.append({'action': act, 'f0': a, 'f1': b, 'P': P, 'speed': d.length / ((b - a) / fps), 'drift': d})
if not sections:
    raise SystemExit('LZ_RIG: no moving section found')
sections.sort(key=lambda s: s['speed'])
if len(sections) == 1:
    sections[0]['name'] = 'Run' if sections[0]['speed'] / height > 1.2 else 'Walk'
else:
    sections = [sections[0], sections[-1]]
    sections[0]['name'], sections[1]['name'] = 'Walk', 'Run'

# ---- bake in-place copies
clips = []
inv = arm.matrix_world.inverted().to_3x3()
rest_hips = arm.matrix_world @ hips.bone.head_local
for s in sections:
    new = s['action'].copy(); new.name = s['name']; new.use_fake_user = True
    arm.animation_data.action = new
    n = s['f1'] - s['f0']
    start = s['P'][s['f0']] - rest_hips; start.z = 0   # clip may start away from the origin
    for f in range(s['f0'], s['f1'] + 1):
        sc.frame_set(f)
        off = inv @ (start + s['drift'] * ((f - s['f0']) / n))
        M = hips.matrix.copy(); M.translation -= off
        hips.matrix = M
        bpy.context.view_layer.update()
        hips.keyframe_insert('location', frame=f)
    new.use_frame_range = True
    new.frame_start, new.frame_end = s['f0'], s['f1']
    # residual drift check
    p0, p1 = hips_world(s['f0']), hips_world(s['f1'])
    centre = sum((hips_world(f) - rest_hips for f in range(s['f0'], s['f1'] + 1)), Vector()) / (n + 1)
    clips.append({'name': s['name'], 'source_take': s['action'].name, 'frames': [s['f0'], s['f1']], 'seconds': round(n / fps, 3),
                  'root_speed_mps': round(s['speed'], 4), 'root_speed_per_height': round(s['speed'] / height, 3),
                  'residual_drift_m': round((p1 - p0).to_2d().length, 4),
                  'mean_hips_offset_m': [round(x, 3) for x in centre.to_2d()]})
for act in src_actions:
    bpy.data.actions.remove(act)
arm.animation_data.action = bpy.data.actions[clips[-1]['name']]

# ---- normalise the armature's object scale (Mixamo/Tripo FBX = 0.01) so importers don't apply it twice.
def fcurves_of(act):
    try:
        return list(act.fcurves)
    except AttributeError:
        pass
    out = []
    for layer in act.layers:
        for strip in layer.strips:
            for slot in act.slots:
                cb = strip.channelbag(slot)
                if cb:
                    out += list(cb.fcurves)
    return out
s_fix = arm.scale.x
probe = next(b for b in arm.pose.bones if b.name.lower().endswith('leftfoot'))
mid_f = (clips[-1]['frames'][0] + clips[-1]['frames'][1]) // 2
sc.frame_set(mid_f); before_fix = (arm.matrix_world @ probe.head).copy()
if abs(s_fix - 1.0) > 1e-6:
    for act in bpy.data.actions:
        for fc in fcurves_of(act):
            if fc.data_path.endswith('.location'):
                for kp in fc.keyframe_points:
                    kp.co.y *= s_fix; kp.handle_left.y *= s_fix; kp.handle_right.y *= s_fix
                fc.update()
    bpy.ops.object.select_all(action='DESELECT')
    arm.select_set(True)
    for m in meshes: m.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
sc.frame_set(mid_f); after_fix = (arm.matrix_world @ probe.head).copy()
scale_fix = {'armature_scale_was': round(s_fix, 5), 'pose_error_m': round((after_fix - before_fix).length, 5)}

# ---- materials: base colour only, resized
tex_info = []
for mat in bpy.data.materials:
    if not mat.use_nodes: continue
    nt = mat.node_tree
    bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if not bsdf: continue
    for inp in ('Normal', 'Roughness', 'Metallic', 'Specular IOR Level', 'Alpha'):
        if inp in bsdf.inputs:
            for l in list(bsdf.inputs[inp].links): nt.links.remove(l)
    for l in bsdf.inputs['Base Color'].links:
        n = l.from_node
        if n.type == 'TEX_IMAGE' and n.image:
            before = list(n.image.size)
            if max(before) > TEX: n.image.scale(TEX, TEX)
            n.image.pack()
            tex_info.append({'image': n.image.name, 'before': before, 'after': list(n.image.size)})

os.makedirs(os.path.dirname(os.path.abspath(DST)), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=DST, export_format='GLB', export_image_format='AUTO', export_jpeg_quality=90,
                          export_animations=True, export_animation_mode='ACTIONS', export_force_sampling=True,
                          export_skins=True, export_def_bones=False, export_materials='EXPORT', export_tangents=False)

if PREVIEW:
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'FLAT'; sc.display.shading.color_type = 'TEXTURE'
    sc.render.resolution_x, sc.render.resolution_y = 512, 640
    sc.world = sc.world or bpy.data.worlds.new('w')
    cd = bpy.data.cameras.new('cam'); cd.type = 'ORTHO'; cd.ortho_scale = height * 1.25
    cam = bpy.data.objects.new('cam', cd); sc.collection.objects.link(cam); sc.camera = cam
    base, ext = os.path.splitext(PREVIEW)
    shots = [('rest_front', None, 0, (0, -10, height / 2), (math.radians(90), 0, 0))]
    for c in clips:
        mid = (c['frames'][0] + c['frames'][1]) // 2
        shots += [(f"{c['name'].lower()}_side", c['name'], mid, (10, 0, height / 2), (math.radians(90), 0, math.radians(90))),
                  (f"{c['name'].lower()}_front", c['name'], c['frames'][0] + 3, (0, -10, height / 2), (math.radians(90), 0, 0))]
    for tag, clip, f, loc, rot in shots:
        arm.animation_data.action = bpy.data.actions[clip] if clip else None
        if not clip:
            for pb in arm.pose.bones: pb.matrix_basis.identity()
        sc.frame_set(f)
        cam.location, cam.rotation_euler = loc, rot
        sc.render.filepath = f'{base}_{tag}{ext}'
        bpy.ops.render.render(write_still=True)

stats = {'params': {'tex': TEX}, 'scale_fix': scale_fix, 'source': os.path.basename(SRC), 'output': os.path.basename(DST), 'fps': fps, 'tris': tris,
         'height_m_native': round(height, 4), 'bones': len(arm.data.bones), 'clips': clips, 'textures': tex_info,
         'output_bytes': os.path.getsize(DST), 'seconds': round(time.time() - t0, 1)}
print('LZ_RIG_STATS ' + json.dumps(stats))
if STATS:
    with open(STATS, 'w') as f: json.dump(stats, f, indent=2)
