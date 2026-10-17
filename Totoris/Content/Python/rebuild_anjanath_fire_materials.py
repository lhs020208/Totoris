# -*- coding: utf-8 -*-
"""Totoris - reliable Anjanath fire/smoke material repair (Unreal Editor 5.x).

Run through Unreal Editor: Tools > Execute Python Script.
Requires SourceArt/Anjanath/*.png placed under the directory containing .uproject.
Rebuilds ONLY M_AnjanathFire and M_AnjanathSmoke, imports ONLY the two T_ textures.
Uses built-in material nodes, no fragile MaterialExpressionCustom/HLSL compiler.
The C++ particle system continues to drive the "EffectOpacity" scalar parameter.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.project_dir()
SRC_DIR = os.path.normpath(os.path.join(PROJECT_DIR, "SourceArt", "Anjanath"))
DEST_DIR = "/Game/Totoris/Materials/Events"
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ED = unreal.EditorAssetLibrary


def require_connection(ok, desc):
    if ok is False:
        raise RuntimeError("Material graph connection FAILED: " + desc)


def wire(from_expr, output_name, to_expr, input_name):
    require_connection(
        LIB.connect_material_expressions(from_expr, output_name, to_expr, input_name),
        '{}.{} -> {}.{}'.format(from_expr.get_name(), output_name,
                                  to_expr.get_name(), input_name))


def property_wire(from_expr, output_name, material_property):
    require_connection(
        LIB.connect_material_property(from_expr, output_name, material_property),
        '{} -> {}'.format(from_expr.get_name(), material_property))


def expr(material, cls, x, y):
    node = LIB.create_material_expression(material, cls, x, y)
    if not node:
        raise RuntimeError("Could not create material node: " + str(cls))
    return node


def scalar_param(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", float(value))
    return node


def import_sprite(name):
    src = os.path.join(SRC_DIR, name + ".png")
    if not os.path.isfile(src):
        raise FileNotFoundError(
            "Missing PNG: " + src +
            "\nUnzip SourceArt/Anjanath into the Unreal project root beside Totoris.uproject")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", DEST_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    TOOLS.import_asset_tasks([task])
    path = DEST_DIR + "/" + name
    texture = unreal.load_asset(path)
    if not texture:
        raise RuntimeError("PNG import failed: " + src + " -> " + path)
    # Color for the flame/smoke is authored in sRGB; alpha stays a linear mask.
    texture.set_editor_property("srgb", True)
    ED.save_loaded_asset(texture)
    unreal.log("ANJANATH OK: imported " + path)
    return texture


def build_material(name, texture, smoke):
    path = DEST_DIR + "/" + name
    material = unreal.load_asset(path)
    if not material:
        material = TOOLS.create_asset(name, DEST_DIR, unreal.Material,
                                      unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Cannot create material: " + path)

    LIB.delete_all_material_expressions(material)
    # Unlit sprites, billboards are two-sided; smoke alpha mixes into background.
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT
                                 if smoke else unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    sample = expr(material, unreal.MaterialExpressionTextureSampleParameter2D,
                  -900, -160)
    sample.set_editor_property("parameter_name", "SmokeSprite" if smoke else "FireSprite")
    sample.set_editor_property("texture", texture)

    opacity_param = scalar_param(material, "EffectOpacity", 1.0, -900, 220)
    alpha_mul = expr(material, unreal.MaterialExpressionMultiply, -460, 220)
    wire(sample, "A", alpha_mul, "A")
    wire(opacity_param, "", alpha_mul, "B")
    property_wire(alpha_mul, "", unreal.MaterialProperty.MP_OPACITY)

    brightness = scalar_param(material,
                              "SmokeBrightness" if smoke else "FireBrightness",
                              1.0 if smoke else 2.6, -900, -440)
    colored = expr(material, unreal.MaterialExpressionMultiply, -450, -260)
    wire(sample, "RGB", colored, "A")
    wire(brightness, "", colored, "B")

    if smoke:
        # Semi-transparent charcoal: RGBA sprite alpha controls the wispy edge.
        property_wire(colored, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        # Important for ADDITIVE: explicitly premultiply emissive RGB by opacity
        # so transparent PNG areas cannot light up as rectangular card outlines.
        visible_fire = expr(material, unreal.MaterialExpressionMultiply, -130, -100)
        wire(colored, "", visible_fire, "A")
        wire(alpha_mul, "", visible_fire, "B")
        property_wire(visible_fire, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    LIB.recompile_material(material)
    if not ED.save_loaded_asset(material):
        raise RuntimeError("Cannot save material: " + path)
    n = LIB.get_num_material_expressions(material)
    unreal.log("ANJANATH OK: rebuilt {} with {} nodes; {}".format(
        path, n, "Translucent Smoke" if smoke else "Additive Fire"))


def main():
    unreal.log("ANJANATH repair start; project: " + PROJECT_DIR)
    ED.make_directory(DEST_DIR)
    # Check both files before modifying any existing assets.
    for tex_name in ("T_AnjanathFire", "T_AnjanathSmoke"):
        path = os.path.join(SRC_DIR, tex_name + ".png")
        if not os.path.isfile(path):
            raise FileNotFoundError("Required sprite not found: " + path)
    fire_tex = import_sprite("T_AnjanathFire")
    smoke_tex = import_sprite("T_AnjanathSmoke")
    build_material("M_AnjanathFire", fire_tex, smoke=False)
    build_material("M_AnjanathSmoke", smoke_tex, smoke=True)
    unreal.log_warning("ANJANATH MATERIAL REPAIR COMPLETE. "
                       "Open both materials: verify FireSprite/SmokeSprite, Emissive, Opacity; "
                       "then stop/start PIE to refresh dynamic instances.")
    print("ANJANATH MATERIAL REPAIR COMPLETE")


if __name__ == "__main__":
    main()
