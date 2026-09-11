import bpy
from mathutils import Vector


# Game-ready stylized sofa, dimensions in metres: W 2.20 x D 1.05 x H 0.90
OUTFILE = r"D:\GitHub\Totoris\Sofa.blend"
PREVIEW = r"D:\GitHub\Totoris\Sofa_preview.png"


def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials,
                       bpy.data.cameras, bpy.data.lights):
        # Keep materials made below only; unused defaults are safe to remove.
        for item in list(datablocks):
            if item.users == 0:
                datablocks.remove(item)


def material(name, color, roughness=0.62):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1.0)
    bsdf.inputs['Roughness'].default_value = roughness
    return mat


def rounded_box(name, location, dimensions, bevel, mat, segments=3):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel_mod = obj.modifiers.new('Soft_Edges', 'BEVEL')
    bevel_mod.width = bevel
    bevel_mod.segments = segments
    bevel_mod.limit_method = 'ANGLE'
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=bevel_mod.name)
    obj.data.materials.append(mat)
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj


def cushion(name, location, dimensions, mat, rotation=(0, 0, 0), segments=32, rings=16):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = (dimensions[0] / 2, dimensions[1] / 2, dimensions[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj


def cylinder(name, location, radius, depth, mat):
    bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=radius, depth=depth, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    bevel_mod = obj.modifiers.new('Round_Edge', 'BEVEL')
    bevel_mod.width = min(radius * 0.28, 0.025)
    bevel_mod.segments = 2
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=bevel_mod.name)
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj


clear_scene()

ivory = material('Fabric_Ivory', (0.91, 0.875, 0.83), 0.82)
ivory_dark = material('Fabric_Ivory_Shadow', (0.80, 0.765, 0.71), 0.84)
blue = material('Pillow_Blue', (0.35, 0.61, 0.92), 0.72)
blue_light = material('Pillow_Check_Light', (0.70, 0.84, 1.0), 0.75)
pink = material('Pillow_Pink', (1.0, 0.57, 0.70), 0.7)
eye_blue = material('Bunny_Eye_Blue', (0.02, 0.18, 0.85), 0.32)

sofa = bpy.data.collections.new('Sofa')
bpy.context.scene.collection.children.link(sofa)

def put_in_sofa(obj):
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)
    sofa.objects.link(obj)
    return obj

# Main shell: low and wide, with pronounced rounded ends.
put_in_sofa(rounded_box('Sofa_Base', (0, 0.01, 0.245), (2.08, 0.96, 0.30), 0.14, ivory_dark))
put_in_sofa(cushion('Left_Arm_Round', (-0.89, -0.03, 0.49), (0.42, 0.88, 0.66), ivory))
put_in_sofa(cushion('Right_Arm_Round', (0.89, -0.03, 0.49), (0.42, 0.88, 0.66), ivory))
# Arm top rolls soften the recognisable cloud-like silhouette.
put_in_sofa(cushion('Left_Arm_Top', (-0.83, 0.00, 0.66), (0.35, 0.74, 0.38), ivory))
put_in_sofa(cushion('Right_Arm_Top', (0.83, 0.00, 0.66), (0.35, 0.74, 0.38), ivory))

# Seat is split only subtly, like the reference's single broad cushion.
put_in_sofa(rounded_box('Seat_Cushion', (0, -0.12, 0.47), (1.58, 0.62, 0.24), 0.105, ivory, 4))

# Three individual rounded back cushions create the soft scalloped rear.
for i, x in enumerate((-0.50, 0.0, 0.50), 1):
    put_in_sofa(cushion(f'Back_Cushion_{i}', (x, 0.28, 0.68), (0.70, 0.28, 0.47), ivory))

# Four small feet; front ones are visible at normal game-camera height.
for name, x, y in [('Foot_FL', -0.79, -0.35), ('Foot_FR', 0.79, -0.35),
                   ('Foot_BL', -0.79, 0.34), ('Foot_BR', 0.79, 0.34)]:
    put_in_sofa(cylinder(name, (x, y, 0.075), 0.075, 0.15, ivory_dark))

# Decorative cushions (separate, removable game props).
put_in_sofa(cushion('Pillow_Paw_Base', (-0.51, -0.11, 0.67), (0.38, 0.12, 0.34), ivory))
for i, (x, z, size) in enumerate(((-0.51, 0.69, 0.115), (-0.60, 0.78, 0.075),
                                    (-0.51, 0.81, 0.075), (-0.42, 0.78, 0.075)), 1):
    put_in_sofa(cushion(f'Paw_Pad_{i}', (x, -0.176, z), (size, 0.025, size), blue, segments=20, rings=10))

put_in_sofa(rounded_box('Pillow_Cream', (-0.17, -0.07, 0.69), (0.33, 0.12, 0.30), 0.045, ivory, 3))
check = rounded_box('Pillow_Gingham_Base', (0.16, -0.065, 0.69), (0.32, 0.12, 0.29), 0.045, blue_light, 3)
put_in_sofa(check)
# Simple checkered cue on the front, deliberately geometric for a game asset.
for row in range(3):
    for col in range(3):
        if (row + col) % 2 == 0:
            x = 0.16 + (col - 1) * 0.095
            z = 0.69 + (row - 1) * 0.085
            put_in_sofa(rounded_box(f'Gingham_{row}_{col}', (x, -0.135, z), (0.083, 0.012, 0.073), 0.006, blue, 1))

# Bunny pillow: circular face with ears, eyes, cheeks, and a tiny mouth.
put_in_sofa(cushion('Bunny_Pillow_Face', (0.47, -0.10, 0.71), (0.34, 0.13, 0.32), ivory))
for side in (-1, 1):
    put_in_sofa(cushion(f'Bunny_Ear_{side}', (0.47 + side * 0.085, -0.095, 0.91), (0.10, 0.10, 0.22), ivory, rotation=(0, 0, side * 0.14), segments=20, rings=10))
    put_in_sofa(cushion(f'Bunny_Ear_Inner_{side}', (0.47 + side * 0.085, -0.151, 0.91), (0.045, 0.02, 0.14), pink, rotation=(0, 0, side * 0.14), segments=16, rings=8))
for side in (-1, 1):
    put_in_sofa(cushion(f'Bunny_Eye_{side}', (0.47 + side * 0.067, -0.171, 0.74), (0.045, 0.018, 0.065), eye_blue, segments=16, rings=8))
    put_in_sofa(cushion(f'Bunny_Cheek_{side}', (0.47 + side * 0.10, -0.169, 0.68), (0.055, 0.014, 0.035), pink, segments=16, rings=8))
put_in_sofa(cushion('Bunny_Nose', (0.47, -0.174, 0.70), (0.028, 0.016, 0.022), pink, segments=16, rings=8))

put_in_sofa(rounded_box('Pillow_Blue', (0.75, -0.04, 0.71), (0.34, 0.12, 0.34), 0.05, blue, 3))

# Game import helpers: all mesh objects are smooth, named and grouped.
for obj in sofa.objects:
    obj['asset_type'] = 'game_prop'
    obj['source'] = 'reference_sofa'

# Ground-neutral preview camera and lighting (not required by the asset).
bpy.ops.object.camera_add(location=(0.0, -5.25, 1.58))
camera = bpy.context.object
camera.name = 'Preview_Camera'
camera.data.lens = 58
camera.rotation_euler = (1.142, 0, 0.67)
direction = Vector((0, -0.02, 0.54)) - camera.location
camera.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
bpy.context.scene.camera = camera

bpy.ops.object.light_add(type='AREA', location=(0, -2.3, 2.8))
key = bpy.context.object
key.name = 'Preview_Key_Light'
key.data.energy = 800
key.data.shape = 'DISK'
key.data.size = 3.5
key.rotation_euler = (0.45, 0, 0)

bpy.ops.object.light_add(type='AREA', location=(2.4, 1.5, 1.8))
fill = bpy.context.object
fill.name = 'Preview_Fill_Light'
fill.data.energy = 500
fill.data.size = 2.0
fill.rotation_euler = (1.1, 0, 2.2)

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 900
scene.render.resolution_y = 700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = PREVIEW
scene.render.film_transparent = False
scene.world.color = (0.055, 0.055, 0.055)

# World-unit dimensions shown in Blender are metres.
scene.unit_settings.system = 'METRIC'
scene.unit_settings.length_unit = 'METERS'

bpy.ops.wm.save_as_mainfile(filepath=OUTFILE)
bpy.ops.render.render(write_still=True)
print('SAVED', OUTFILE)
print('PREVIEW', PREVIEW)
