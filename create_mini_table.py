import bpy
from math import radians
from mathutils import Vector


# Stylized game prop: 1.10 m diameter x 0.38 m high.
OUTFILE = r"D:\GitHub\Totoris\Totoris\Blender\Mini Table\MiniTable.blend"
PREVIEW = r"D:\GitHub\Totoris\Totoris\Blender\Mini Table\MiniTable_preview.png"


def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)


def make_material(name, color, roughness=0.4, emission=None):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1.0)
    bsdf.inputs['Roughness'].default_value = roughness
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*emission, 1.0)
        bsdf.inputs['Emission Strength'].default_value = 8.0
    return mat


def smooth(obj):
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj


def uv_ellipsoid(name, loc, dims, mat, segments=32, rings=16):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.scale = (dims[0] / 2, dims[1] / 2, dims[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    return smooth(obj)


def rounded_cylinder(name, loc, radius, depth, mat, bevel=0.03, vertices=48):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    modifier = obj.modifiers.new('Soft_Rim', 'BEVEL')
    modifier.width = bevel
    modifier.segments = 3
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    return smooth(obj)


def put_in(collection, obj):
    for old_collection in list(obj.users_collection):
        old_collection.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


clear_scene()
white = make_material('Glossy_White', (0.84, 0.88, 0.96), 0.24)
white_soft = make_material('Body_White', (0.78, 0.82, 0.91), 0.36)
leg_white = make_material('Leg_White', (0.72, 0.76, 0.84), 0.34)
blue_led = make_material('Blue_LED', (0.20, 0.66, 1.0), 0.18, emission=(0.12, 0.58, 1.0))

table = bpy.data.collections.new('MiniTable')
bpy.context.scene.collection.children.link(table)

# Main body: cloud-like low oval body, then a clean raised circular top.
put_in(table, uv_ellipsoid('Table_Body', (0, 0, 0.175), (1.10, 1.10, 0.29), white_soft))
put_in(table, rounded_cylinder('Tabletop', (0, 0, 0.335), 0.55, 0.085, white, bevel=0.032))

# Thin cyan illumination band just beneath the tabletop rim.
bpy.ops.mesh.primitive_torus_add(major_radius=0.522, minor_radius=0.010, major_segments=64, minor_segments=10, location=(0, 0, 0.302))
led_ring = bpy.context.object
led_ring.name = 'LED_Rim_Blue'
led_ring.data.materials.append(blue_led)
put_in(table, led_ring)

# Four short feet, placed under the curved shell.
for name, x, y in [('Foot_FL', -0.34, -0.34), ('Foot_FR', 0.34, -0.34),
                   ('Foot_BL', -0.34, 0.34), ('Foot_BR', 0.34, 0.34)]:
    bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=0.060, depth=0.085, location=(x, y, 0.047))
    foot = bpy.context.object
    foot.name = name
    foot.data.materials.append(leg_white)
    bevel = foot.modifiers.new('Rounded_Foot', 'BEVEL')
    bevel.width = 0.014
    bevel.segments = 2
    bpy.context.view_layer.objects.active = foot
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    put_in(table, smooth(foot))

# Front bunny mark: face plus ears, set just above the exterior surface.
put_in(table, uv_ellipsoid('Bunny_Logo_Face', (0, -0.539, 0.172), (0.115, 0.016, 0.085), blue_led, 20, 12))
for side in (-1, 1):
    ear = uv_ellipsoid(f'Bunny_Logo_Ear_{side}', (side * 0.033, -0.539, 0.242), (0.035, 0.014, 0.080), blue_led, 16, 10)
    ear.rotation_euler[1] = radians(side * 12)
    put_in(table, ear)

# Smaller matching rabbit motif on the tabletop, useful as a readable game-detail.
put_in(table, uv_ellipsoid('Top_Bunny_Face', (0, -0.02, 0.381), (0.070, 0.070, 0.006), blue_led, 20, 10))
for side in (-1, 1):
    ear = uv_ellipsoid(f'Top_Bunny_Ear_{side}', (side * 0.020, -0.02, 0.385), (0.022, 0.040, 0.006), blue_led, 16, 8)
    ear.rotation_euler[2] = radians(side * 14)
    put_in(table, ear)

for obj in table.objects:
    obj['asset_type'] = 'game_prop'
    obj['dimensions_m'] = '1.10 x 1.10 x 0.38'
    obj['source'] = 'reference_mini_table'

# Preview-only camera and neutral lighting.
bpy.ops.object.camera_add(location=(0.0, -2.45, 0.78))
camera = bpy.context.object
camera.name = 'Preview_Camera'
camera.data.lens = 58
camera.rotation_euler = (Vector((0, -0.02, 0.19)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
bpy.context.scene.camera = camera

bpy.ops.object.light_add(type='AREA', location=(-1.4, -1.8, 2.2))
key = bpy.context.object
key.name = 'Preview_Key'
key.data.energy = 650
key.data.shape = 'DISK'
key.data.size = 2.5
key.rotation_euler = (radians(25), 0, radians(-35))

bpy.ops.object.light_add(type='AREA', location=(1.6, 0.5, 1.4))
fill = bpy.context.object
fill.name = 'Preview_Fill'
fill.data.energy = 360
fill.data.size = 2.0
fill.rotation_euler = (radians(60), 0, radians(105))

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 900
scene.render.resolution_y = 700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = PREVIEW
scene.world.color = (0.045, 0.055, 0.085)
scene.unit_settings.system = 'METRIC'
scene.unit_settings.length_unit = 'METERS'

bpy.ops.wm.save_as_mainfile(filepath=OUTFILE)
bpy.ops.render.render(write_still=True)
print('SAVED', OUTFILE)
print('PREVIEW', PREVIEW)
