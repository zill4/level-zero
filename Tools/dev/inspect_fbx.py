import bpy, sys, json, mathutils
argv = sys.argv[sys.argv.index('--') + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=argv[0], automatic_bone_orientation=False)
sc = bpy.context.scene
arms = [o for o in sc.objects if o.type == 'ARMATURE']
meshes = [o for o in sc.objects if o.type == 'MESH']
out = {'objects': [(o.name, o.type, tuple(round(v, 4) for v in o.scale), o.parent.name if o.parent else None) for o in sc.objects]}
out['fps'] = sc.render.fps / sc.render.fps_base
out['actions'] = [(a.name, tuple(a.frame_range), len(a.fcurves) if hasattr(a, 'fcurves') else -1) for a in bpy.data.actions]
a = arms[0]
out['bones'] = [b.name for b in a.data.bones][:12] + ['...', len(a.data.bones)]
tris = 0
for m in meshes:
    m.data.calc_loop_triangles(); tris += len(m.data.loop_triangles)
out['tris'] = tris
# rest bounds
pts = [m.matrix_world @ v.co for m in meshes for v in m.data.vertices]
mn = [min(p[i] for p in pts) for i in range(3)]; mx = [max(p[i] for p in pts) for i in range(3)]
out['rest_bounds_min'] = [round(x, 3) for x in mn]; out['rest_bounds_max'] = [round(x, 3) for x in mx]
hips = next(b for b in a.pose.bones if b.name.lower().endswith('hips'))
head = next((b for b in a.pose.bones if b.name.lower().endswith(':head') or b.name.lower() == 'head'), None)
out['head_rest_world'] = [round(x, 3) for x in (a.matrix_world @ a.data.bones[head.name].head_local)] if head else None
# forward: nose/face vertices? use LeftFoot toe direction: toe minus foot
toe = next((b for b in a.data.bones if 'lefttoebase' in b.name.lower()), None)
foot = next((b for b in a.data.bones if b.name.lower().endswith('leftfoot')), None)
if toe and foot:
    d = (a.matrix_world @ toe.head_local) - (a.matrix_world @ foot.head_local)
    out['toe_dir_world'] = [round(x, 3) for x in d]
drift = {}
for act in bpy.data.actions:
    a.animation_data_create(); a.animation_data.action = act
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    sc.frame_set(f0); p0 = a.matrix_world @ hips.head
    samples = []
    for f in range(f0, f1 + 1, max(1, (f1 - f0) // 8)):
        sc.frame_set(f); samples.append([round(x, 3) for x in (a.matrix_world @ hips.head)])
    sc.frame_set(f1); p1 = a.matrix_world @ hips.head
    drift[act.name] = {'frames': (f0, f1), 'sec': round((f1 - f0) / out['fps'], 3), 'disp': [round(x, 3) for x in (p1 - p0)], 'samples': samples}
    try:
        slots = [s.identifier for s in act.slots]
    except Exception:
        slots = None
    drift[act.name]['slots'] = slots
    try:
        names = sorted({fc.data_path.split('"')[1] for fc in act.fcurves if '"' in fc.data_path})[:3]
        drift[act.name]['fc_bones'] = names
    except Exception as e:
        drift[act.name]['fc_err'] = str(e)
out['drift'] = drift
print('LZ_FBX ' + json.dumps(out))
