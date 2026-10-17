# -*- coding: utf-8 -*-
"""Totoris UE5.8: Fix a nearly invisible fire sprite while preserving smoke.

Tools > Execute Python Script, stop PIE first.
Uses the EXISTING T_AnjanathFire texture; no reimport or HLSL required.
Repairs ONLY M_AnjanathFire. Recreates its expressions, so back up any
manual customizations to this material first.
"""
import unreal

LIB = unreal.MaterialEditingLibrary
ED = unreal.EditorAssetLibrary
ASSET_ROOT = "/Game/Totoris/Materials/Events"


def required(value, message):
    if value is False or value is None:
        raise RuntimeError("ANJANATH FIRE FIX FAILED: " + message)


def wire(src, output, dst, inp):
    required(LIB.connect_material_expressions(src, output, dst, inp),
             "connecting {}.{} to {}.{}".format(src.get_name(), output,
                                                   dst.get_name(), inp))


def wire_property(src, output, dest):
    required(LIB.connect_material_property(src, output, dest),
             "connecting {} to {}".format(src.get_name(), dest))


def node(mat, kind, x, y):
    result = LIB.create_material_expression(mat, kind, x, y)
    required(result, "creating " + str(kind))
    return result


def main():
    texture_path = ASSET_ROOT + "/T_AnjanathFire"
    material_path = ASSET_ROOT + "/M_AnjanathFire"
    tex = unreal.load_asset(texture_path)
    mat = unreal.load_asset(material_path)
    if not tex or not mat:
        raise RuntimeError("Missing existing assets: {} or {}".format(texture_path, material_path))

    # Additive already blends the emissive color using opacity. Multiplying
    # alpha into the emissive branch as well applies the mask twice, and the
    # resulting flames can become almost invisible on a bright scene.
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    sample = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -800, -140)
    sample.set_editor_property("parameter_name", "FireSprite")
    sample.set_editor_property("texture", tex)

    opacity = node(mat, unreal.MaterialExpressionScalarParameter, -800, 230)
    opacity.set_editor_property("parameter_name", "EffectOpacity")
    opacity.set_editor_property("default_value", 1.0)
    alpha = node(mat, unreal.MaterialExpressionMultiply, -410, 240)
    wire(sample, "A", alpha, "A")
    wire(opacity, "", alpha, "B")
    wire_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)

    bright = node(mat, unreal.MaterialExpressionScalarParameter, -790, -430)
    bright.set_editor_property("parameter_name", "FireBrightness")
    bright.set_editor_property("default_value", 5.0)
    emissive = node(mat, unreal.MaterialExpressionMultiply, -380, -210)
    wire(sample, "RGB", emissive, "A")
    wire(bright, "", emissive, "B")
    wire_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # IMPORTANT: no connection from 'alpha' to the emissive branch.

    LIB.recompile_material(mat)
    required(ED.save_loaded_asset(mat), "saving " + material_path)
    count = LIB.get_num_material_expressions(mat)
    required(count == 5, "unexpected node count: " + str(count))
    unreal.log_warning("ANJANATH FIRE FIX SUCCESS: material rebuilt (5 nodes). "
                       "Now rebuild the patched C++ and restart Unreal Editor/PIE.")
    print("ANJANATH FIRE FIX SUCCESS; material: " + material_path)


if __name__ == "__main__":
    main()
