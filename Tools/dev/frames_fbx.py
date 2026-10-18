import bpy, sys
argv = sys.argv[sys.argv.index('--') + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=argv[0])
sc = bpy.context.scene
a = [o for o in sc.objects if o.type == 'ARMATURE'][0]
act = bpy.data.actions[0]
P = lambda n: next(b for b in a.pose.bones if b.name.endswith(n))
hips, lf, rf, head = P(':Hips'), P(':LeftFoot'), P(':RightFoot'), P(':Head')
W = lambda b: a.matrix_world @ b.head
print('LZ_FR scene', sc.frame_start, sc.frame_end, 'action', tuple(act.frame_range))
prev = None
for f in range(int(act.frame_range[0]) - 2, int(act.frame_range[1]) + 3):
    sc.frame_set(f)
    h = W(hips)
    v = (h - prev).length * 24 if prev is not None else 0
    prev = h.copy()
    print(f'LZ_FR {f:3d} hips y={h.y:7.3f} z={h.z:6.3f} v={v:5.2f}m/s  Lfoot z={W(lf).z:6.3f} y={W(lf).y:7.3f}  Rfoot z={W(rf).z:6.3f} y={W(rf).y:7.3f} head z={W(head).z:6.3f}')
