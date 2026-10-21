"""Run with Unreal Editor > Tools > Execute Python Script (not system Python).
Creates/upgrades M_AnjanathFire and M_AnjanathSmoke. UE 5.x editor required.
"""
import unreal

DIR = "/Game/Totoris/Materials/Events"
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()


def connect(source, target, input_name=""):
    # An unnamed input selects the first pin (ComponentMask has no "Input" pin).
    if not lib.connect_material_expressions(source, "", target, input_name):
        raise RuntimeError("Material connection failed: " + target.get_name() + "." + input_name)

# A soft, tapered procedural flame: orange/red outer rim, yellow-hot turbulent core.
# 2D UV coordinates are smooth (not pixel/dot shaped); alpha is feathered to zero.
FIRE_HLSL = r"""
float2 p = UV * 2.0 - 1.0;
float v = saturate(UV.y);
float w = 0.58 + 0.11 * sin(v * 9.5 + Phase * 5.0 + Age * 6.0);
w *= 1.0 - 0.78 * pow(v, 1.7);
float sideShift = 0.10 * sin(v * 14.0 + Phase * 2.8 + Age * 5.5)
                + 0.045 * sin(v * 31.0 + Phase * 8.2 - Age * 12.0);
float s = abs(p.x - sideShift) / max(0.08, w);
float edge = 1.0 - smoothstep(0.62, 1.12, s);
float heightMask = smoothstep(0.01, 0.19, v) * (1.0 - smoothstep(0.70, 1.0, v));
float turbulence = 0.79 + 0.12 * sin(p.x * 17.0 + v * 24.0 + Phase * 4.0 + Age * 13.0)
                       + 0.09 * cos(v * 37.0 - p.x * 19.0 + Phase * 11.0 - Age * 8.0);
// Nearly opaque inside the original flame silhouette, still feathered at its boundary.
// Keep the particle lifetime fade so spawn/despawn does not pop.
float flameMask = saturate(edge * heightMask * turbulence);
float alpha = 0.97 * smoothstep(0.0, 0.35, flameMask) * saturate(Opacity);
float core = (1.0 - smoothstep(0.05, 0.72, s)) * (1.0 - smoothstep(0.48, 0.92, v));
float3 redOrange = float3(1.25, 0.085, 0.006);
float3 hotOrange = float3(3.3, 0.78, 0.036);
float3 yellow = float3(5.8, 2.4, 0.32);
float3 col = lerp(redOrange, hotOrange, saturate(turbulence * edge));
col = lerp(col, yellow, saturate(core * 0.83));
// Translucent blending applies alpha itself; premultiplying here darkens overlapping edges.
return float4(col, alpha);
"""

# Translucent black/charcoal smoke. FBM-like wisps and soft radial fade.
SMOKE_HLSL = r"""
float2 p = UV * 2.0 - 1.0;
float2 q = p + float2(0.11 * sin(p.y * 6.0 + Phase * 2.0 + Age * 1.7),
                       0.08 * cos(p.x * 7.0 + Phase * 3.0 - Age * 1.3));
float r = length(q);
float n = 0.5 + 0.25 * sin(q.x * 9.0 + q.y * 5.0 + Phase * 4.0 + Age * 2.0)
              + 0.13 * cos(q.y * 16.0 - q.x * 11.0 + Phase * 7.0 - Age * 1.4)
              + 0.09 * sin(q.x * 23.0 + q.y * 19.0 + Phase * 10.0 + Age);
float cloud = 1.0 - smoothstep(0.27, 1.04, r + (n - 0.5) * 0.28);
float detail = saturate(0.63 + 0.45 * n);
float alpha = cloud * detail * Opacity * 0.22;
float3 charcoal = lerp(float3(0.014, 0.016, 0.023),
                       float3(0.070, 0.071, 0.080), saturate(n));
return float4(charcoal, alpha);
"""


def make_material(name, blend_mode, shader_code):
    path = DIR + "/" + name
    mat = unreal.load_asset(path)
    if not mat:
        mat = assets.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("Failed to create material: " + path)

    lib.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", blend_mode)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    tex = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    phase = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
    phase.set_editor_property("parameter_name", "EffectPhase")
    age = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
    age.set_editor_property("parameter_name", "EffectAge")
    opacity = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
    opacity.set_editor_property("parameter_name", "EffectOpacity")
    opacity.set_editor_property("default_value", 1.0)

    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom_inputs = []
    for nm in ("UV", "Phase", "Age", "Opacity"):
        inp = unreal.CustomInput()
        inp.set_editor_property("input_name", nm)
        custom_inputs.append(inp)
    custom.set_editor_property("inputs", custom_inputs)
    custom.set_editor_property("code", shader_code)
    connect(tex, custom, "UV")
    connect(phase, custom, "Phase")
    connect(age, custom, "Age")
    connect(opacity, custom, "Opacity")

    rgb = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
    rgb.set_editor_property("r", True)
    rgb.set_editor_property("g", True)
    rgb.set_editor_property("b", True)
    rgb.set_editor_property("a", False)
    connect(custom, rgb)
    alpha = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
    alpha.set_editor_property("r", False)
    alpha.set_editor_property("g", False)
    alpha.set_editor_property("b", False)
    alpha.set_editor_property("a", True)
    connect(custom, alpha)

    emissive = rgb
    if name == "M_AnjanathFire":
        # Match the Minecraft fire's exposure-independent emission. Unlit alone
        # still passes through the level's exposure, unlike the material preview.
        inverse = lib.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptationInverse)
        strength = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
        strength.set_editor_property("r", 1.0)
        connect(rgb, inverse, "LightValueInput")
        connect(strength, inverse, "AlphaInput")
        emissive = inverse
    if not lib.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Emissive connection failed: " + path)
    if not lib.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Opacity connection failed: " + path)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat):
        raise RuntimeError("Could not save: " + path)
    unreal.log("Anjanath fire effect material saved: " + path)
    return mat


make_material("M_AnjanathFire", unreal.BlendMode.BLEND_TRANSLUCENT, FIRE_HLSL)
make_material("M_AnjanathSmoke", unreal.BlendMode.BLEND_TRANSLUCENT, SMOKE_HLSL)
print("Anjanath procedural fire and smoke materials created successfully.")
