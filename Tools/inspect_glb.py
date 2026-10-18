# blender -b --factory-startup -P inspect_glb.py -- <file.glb>
import bpy, sys, mathutils
src = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
tris = 0
lo = mathutils.Vector((1e9,)*3); hi = -lo
for o in bpy.context.scene.objects:
    print('OBJ', o.name, o.type, tuple(round(v,3) for v in o.dimensions), 'parent', o.parent.name if o.parent else None)
    if o.type == 'MESH':
        me = o.data
        me.calc_loop_triangles()
        tris += len(me.loop_triangles)
        print('  verts', len(me.vertices), 'tris', len(me.loop_triangles), 'uvs', [u.name for u in me.uv_layers], 'mats', [m.name for m in me.materials])
        for v in o.bound_box:
            w = o.matrix_world @ mathutils.Vector(v)
            lo = mathutils.Vector(map(min, lo, w)); hi = mathutils.Vector(map(max, hi, w))
print('TOTAL_TRIS', tris, 'BOUNDS', tuple(round(x,3) for x in lo), tuple(round(x,3) for x in hi))
for m in bpy.data.materials:
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image:
                links = [l.to_socket.name for l in n.outputs[0].links]
                print('MAT', m.name, 'TEX', n.image.name, n.image.size[:], '->', links)
