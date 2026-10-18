"""Blender (headless) Tripo GLB reducer for Level Zero (mobile XR budget).
blender -b --factory-startup -P Tools/tripo_reduce.py -- --in X.glb --out Y.glb [--tris 30000] [--tex 2048]
        [--height 2.6 | --longest 1.2] [--rot x,y,z] [--preview Y_preview.png] [--stats Y.json]
        [--card wall|ceiling] [--blur px]
- --card: for flush-mounted props (vents, light panels). The mesh is aligned by PCA, its visible face is baked
  to an albedo texture (orthographic render) and the output is a single textured quad (2 tris), facing -Y (wall,
  i.e. UE +X) or down (ceiling).
- --blur: soften the base colour (e.g. pseudo-text on labels) by a box blur of N pixels after resizing.
- joins all meshes, decimates (collapse) to ~--tris triangles, keeps UVs
- keeps ONLY the base colour texture (Level Zero materials are unlit), resized to --tex (power of two)
- optional uniform scale so the model is --height metres tall; pivot moved to the bottom centre (feet)
- writes a GLB with the texture embedded, an optional front/side preview render and a JSON stats file"""
import bpy, sys, json, os, math, mathutils, time

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(name, default=None, cast=str):
    return cast(argv[argv.index(name) + 1]) if name in argv else default
SRC, DST = arg('--in'), arg('--out')
TRIS, TEX = arg('--tris', 30000, int), arg('--tex', 2048, int)
HEIGHT = arg('--height', None, float)
LONGEST = arg('--longest', None, float)
ROT = arg('--rot', None)  # 'x,y,z' degrees, applied before scaling (fix models that aren't upright / front=-Y)
PREVIEW, STATS = arg('--preview'), arg('--stats')
CARD = arg('--card')
BLUR = arg('--blur', 0.0, float)
assert SRC and DST, 'need --in and --out'
t0 = time.time()

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert meshes, 'no meshes in ' + SRC
# Apply parent transforms and join into one object.
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
for o in [o for o in bpy.context.scene.objects if o != obj]:
    bpy.data.objects.remove(o, do_unlink=True)

def tri_count(o):
    o.data.calc_loop_triangles()
    return len(o.data.loop_triangles)
tris_before = tri_count(obj)

# Merge exact duplicate verts first (Tripo meshes are often split per-UV-island), then decimate.
if tris_before > TRIS and not CARD:
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=1e-6)
    bpy.ops.object.mode_set(mode='OBJECT')
    mod = obj.modifiers.new('lz_decimate', 'DECIMATE')
    mod.decimate_type = 'COLLAPSE'
    mod.use_collapse_triangulate = True
    mod.ratio = max(min(TRIS / max(tri_count(obj), 1), 1.0), 0.0005)
    bpy.ops.object.modifier_apply(modifier=mod.name)
tris_after = tri_count(obj)

if ROT:
    rx, ry, rz = (math.radians(float(v)) for v in ROT.split(','))
    obj.data.transform(mathutils.Euler((rx, ry, rz), 'XYZ').to_matrix().to_4x4())
    obj.data.update()
    bpy.context.view_layer.update()

if CARD:
    import numpy as np
    co = np.array([tuple(v.co) for v in obj.data.vertices])
    cen = co.mean(0)
    w_, V = np.linalg.eigh(np.cov((co - cen).T))   # ascending: normal, middle, long
    n, l = V[:, 0], V[:, 2]
    if n @ np.array([0, -1, 0]) < 0:               # the face seen from the front (-Y) is the visible one
        n = -n
    m = np.cross(n, l)
    Nt = np.array([0, -1, 0]) if CARD == 'wall' else np.array([0, 0, -1])
    X = np.array([1, 0, 0]); Mt = np.cross(Nt, X)
    R = np.column_stack([X, Mt, Nt]) @ np.column_stack([l, m, n]).T
    M4 = mathutils.Matrix(R.tolist()).to_4x4() @ mathutils.Matrix.Translation(-mathutils.Vector(cen))
    obj.data.transform(M4); obj.data.update(); bpy.context.view_layer.update()
    vs = [v.co for v in obj.data.vertices]
    x0, x1 = min(v.x for v in vs), max(v.x for v in vs)
    a_ = 2 if CARD == 'wall' else 1                 # in-plane second axis: Z (wall) or Y (ceiling)
    b0, b1 = min(v[a_] for v in vs), max(v[a_] for v in vs)
    face = min(v[1] for v in vs) if CARD == 'wall' else min(v.z for v in vs)
    # Render exactly the panel rectangle (no background to bleed into mips), then stretch to a square texture.
    S = max(x1 - x0, b1 - b0)
    cx, cb = (x0 + x1) / 2, (b0 + b1) / 2
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'FLAT'; sc.display.shading.color_type = 'TEXTURE'
    sc.display.shading.show_cavity = False; sc.display.shading.show_object_outline = False
    sc.view_settings.view_transform = 'Standard'
    sc.world = sc.world or bpy.data.worlds.new('w'); sc.world.color = (0.05, 0.05, 0.05)
    ar = (b1 - b0) / max(x1 - x0, 1e-6)
    sc.render.resolution_x, sc.render.resolution_y = (TEX, max(int(round(TEX * ar)), 8)) if ar <= 1 else (max(int(round(TEX / ar)), 8), TEX)
    sc.render.film_transparent = False
    cd = bpy.data.cameras.new('bake'); cd.type = 'ORTHO'; cd.ortho_scale = S; cd.clip_end = 100
    cam = bpy.data.objects.new('bake', cd); sc.collection.objects.link(cam); sc.camera = cam
    if CARD == 'wall':
        cam.location, cam.rotation_euler = (cx, face - 5, cb), (math.radians(90), 0, 0)
    else:
        cam.location, cam.rotation_euler = (cx, cb, face - 5), (math.radians(180), 0, 0)
    bake_path = os.path.splitext(os.path.abspath(DST))[0] + '_card.png'
    sc.render.image_settings.file_format = 'PNG'
    sc.render.filepath = bake_path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    # Replace the mesh with one quad carrying the bake.
    bake_img = bpy.data.images.load(bake_path)
    bake_img.scale(TEX, TEX)
    U = lambda x: (x - x0) / max(x1 - x0, 1e-6)
    Vb = lambda b: (b - b0) / max(b1 - b0, 1e-6)
    if CARD == 'wall':
        P = [(x0, face, b0), (x1, face, b0), (x1, face, b1), (x0, face, b1)]
        UV = [(U(p[0]), Vb(p[2])) for p in P]
    else:
        P = [(x0, b0, face), (x0, b1, face), (x1, b1, face), (x1, b0, face)]
        UV = [(U(p[0]), 1.0 - Vb(p[1])) for p in P]
    me = bpy.data.meshes.new('card'); me.from_pydata(P, [], [(0, 1, 2, 3)]); me.update()
    uvl = me.uv_layers.new(name='UVMap')
    for i, loop in enumerate(me.loops):
        uvl.data[i].uv = UV[loop.vertex_index]
    mat = bpy.data.materials.new(os.path.basename(DST)[:-4] + '_card'); mat.use_nodes = True
    bsdf = next(nd for nd in mat.node_tree.nodes if nd.type == 'BSDF_PRINCIPLED')
    tn = mat.node_tree.nodes.new('ShaderNodeTexImage'); tn.image = bake_img
    mat.node_tree.links.new(tn.outputs['Color'], bsdf.inputs['Base Color'])
    me.materials.append(mat)
    old = obj
    obj = bpy.data.objects.new(old.name, me); sc.collection.objects.link(obj)
    bpy.data.objects.remove(old, do_unlink=True)
    bpy.context.view_layer.objects.active = obj
    bpy.context.view_layer.update()
    tris_after = 2

# Scale + pivot at bottom centre.
bb = [obj.matrix_world @ mathutils.Vector(c) for c in obj.bound_box]
lo = mathutils.Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
hi = mathutils.Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
s = (HEIGHT / (hi.z - lo.z)) if HEIGHT else ((LONGEST / max(hi - lo)) if LONGEST else 1.0)
centre = mathutils.Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
obj.data.transform(mathutils.Matrix.Translation(-centre))
obj.data.transform(mathutils.Matrix.Scale(s, 4))
obj.location = (0, 0, 0)
bpy.context.view_layer.update()
dims = tuple(round(d, 3) for d in obj.dimensions)

# Materials: keep only base colour, resized; drop normal / roughness-metal maps.
tex_info = []
for mat in obj.data.materials:
    if not mat or not mat.use_nodes:
        continue
    nt = mat.node_tree
    bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    base_img = None
    if bsdf and bsdf.inputs['Base Color'].links:
        src_node = bsdf.inputs['Base Color'].links[0].from_node
        if src_node.type == 'TEX_IMAGE':
            base_img = src_node.image
    for n in list(nt.nodes):
        if n.type == 'TEX_IMAGE' and n.image is not base_img:
            nt.nodes.remove(n)
        elif n.type in ('NORMAL_MAP', 'SEPARATE_COLOR', 'SEPRGB', 'SEPARATE_RGB'):
            nt.nodes.remove(n)
    if bsdf:
        bsdf.inputs['Metallic'].default_value = 0.0
        bsdf.inputs['Roughness'].default_value = 1.0
    if base_img:
        w, h = base_img.size
        before = (w, h)
        if max(w, h) > TEX:
            base_img.scale(TEX, TEX)
        if BLUR > 0:
            import numpy as np
            W_, H_ = base_img.size
            px = np.empty(W_ * H_ * 4, dtype=np.float32); base_img.pixels.foreach_get(px)
            px = px.reshape(H_, W_, 4); r = max(int(round(BLUR)), 1)
            for _ in range(2):  # two box passes ~ gaussian
                for ax in (0, 1):
                    acc = np.cumsum(np.pad(px, [(r + 1, r) if a == ax else (0, 0) for a in range(3)], mode='edge'), axis=ax)
                    hi_ = np.take(acc, range(2 * r + 1, acc.shape[ax]), axis=ax); lo_ = np.take(acc, range(0, acc.shape[ax] - 2 * r - 1), axis=ax)
                    px = (hi_ - lo_) / (2 * r + 1)
            base_img.pixels.foreach_set(px.astype(np.float32).ravel()); base_img.update()
        base_img.file_format = 'PNG' if base_img.alpha_mode != 'NONE' and base_img.depth == 32 else 'JPEG'
        base_img.pack()
        tex_info.append({'material': mat.name, 'image': base_img.name, 'before': before, 'after': tuple(base_img.size)})

bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
os.makedirs(os.path.dirname(os.path.abspath(DST)), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=DST, export_format='GLB', use_selection=True, export_image_format='AUTO',
                          export_jpeg_quality=90, export_normals=True, export_tangents=False, export_materials='EXPORT',
                          export_apply=True)

if PREVIEW:
    # Front (-Y) and right (+X) orthographic views, textured, flat lighting.
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'FLAT'
    scene.display.shading.color_type = 'TEXTURE'
    scene.render.resolution_x, scene.render.resolution_y = 512, 768
    scene.render.film_transparent = False
    scene.world = bpy.data.worlds.new('w') if not scene.world else scene.world
    zc = obj.dimensions.z / 2
    cam_data = bpy.data.cameras.new('cam'); cam_data.type = 'ORTHO'; cam_data.ortho_scale = obj.dimensions.z * 1.1
    cam = bpy.data.objects.new('cam', cam_data); scene.collection.objects.link(cam); scene.camera = cam
    base, ext = os.path.splitext(PREVIEW)
    for tag, loc, rot in (('front', (0, -10, zc), (math.radians(90), 0, 0)), ('right', (10, 0, zc), (math.radians(90), 0, math.radians(90))), ('back', (0, 10, zc), (math.radians(90), 0, math.radians(180)))):
        cam.location, cam.rotation_euler = loc, rot
        scene.render.filepath = f'{base}_{tag}{ext}'
        bpy.ops.render.render(write_still=True)

stats = {'params': {'tris': TRIS, 'tex': TEX, 'height': HEIGHT, 'longest': LONGEST, 'rot': ROT, 'card': CARD, 'blur': BLUR or None, 'card_v': 2 if CARD else None}, 'source': os.path.basename(SRC), 'output': os.path.basename(DST), 'tris_before': tris_before, 'tris_after': tris_after,
         'dimensions_m': dims, 'scale_applied': round(s, 4), 'textures': tex_info, 'seconds': round(time.time() - t0, 1),
         'output_bytes': os.path.getsize(DST)}
print('LZ_TRIPO_STATS ' + json.dumps(stats))
if STATS:
    with open(STATS, 'w') as f:
        json.dump(stats, f, indent=2)
