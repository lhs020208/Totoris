"""Import the original animation strips without resampling or changing pixels."""
import json
import struct
from pathlib import Path
import unreal

source = Path(r"D:\GitHub\Totoris\Totoris\Image\Minecraft\Fire")
texture_dir = "/Game/Totoris/Objects/Textures/Minecraft"
material_dir = "/Game/Totoris/Objects/Materials/Minecraft"
assets = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
for i in range(2):
    filename = source / f"fire_{i}.png"
    width, height = struct.unpack(">II", filename.read_bytes()[16:24])
    rows = height // width
    meta = json.loads(Path(str(filename) + ".mcmeta").read_text())["animation"]
    frames = meta.get("frames", list(range(rows)))
    assert frames == [(j + frames[0]) % rows for j in range(rows)]
    assert meta.get("frametime", 1) == 1
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = texture_dir
    task.destination_name = f"T_MinecraftFire{i}"
    task.automated = True
    task.replace_existing = False
    task.save = True
    texture = unreal.load_asset(texture_dir + "/" + task.destination_name)
    if not texture:
        assets.import_asset_tasks([task])
        texture = unreal.load_asset(texture_dir + "/" + task.destination_name)
    assert texture
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    name = f"M_MinecraftEntityFire{i}"
    mat = unreal.load_asset(material_dir + "/" + name)
    if not mat:
        mat = assets.create_asset(name, material_dir, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("opacity_mask_clip_value", .1)
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    frame = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
    frame.set_editor_property("parameter_name", "Frame")
    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    inputs = []
    for name in ("UV", "Frame"):
        inp = unreal.CustomInput()
        inp.set_editor_property("input_name", name)
        inputs.append(inp)
    custom.set_editor_property("inputs", inputs)
    custom.set_editor_property("code", f"return float2(clamp(UV.x,0.5/{width},1.0-0.5/{width}), (fmod(floor(Frame)+{frames[0]},{rows})+clamp(UV.y,0.5/{width},1.0-0.5/{width}))/{rows});")
    sample = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property("texture", texture)
    lib.connect_material_expressions(uv, "", custom, "UV")
    lib.connect_material_expressions(frame, "", custom, "Frame")
    lib.connect_material_expressions(custom, "", sample, "UVs")
    inverse = lib.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptationInverse)
    alpha = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
    alpha.set_editor_property("r", 1.0)
    assert lib.connect_material_expressions(sample, "RGB", inverse, "LightValueInput")
    assert lib.connect_material_expressions(alpha, "", inverse, "AlphaInput")
    lib.connect_material_property(inverse, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"Minecraft fire {i}: {width}x{height}, {rows} frames, 20fps, offset {frames[0]}")
