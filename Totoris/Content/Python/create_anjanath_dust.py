import unreal

path = "/Game/Totoris/Materials/Events/M_AnjanathDust"
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_AnjanathDust", "/Game/Totoris/Materials/Events", unreal.Material, unreal.MaterialFactoryNew())
lib = unreal.MaterialEditingLibrary
lib.delete_all_material_expressions(mat)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
mask = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
mask.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
inp = unreal.CustomInput()
inp.set_editor_property("input_name", "UV")
mask.set_editor_property("inputs", [inp])
mask.set_editor_property("code", """
float2 p = UV * 2.0 - 1.0;
float r = length(p);
float cloud = 0.72 + 0.13*sin(p.x*11+p.y*7) + 0.10*cos(p.y*16-p.x*5);
return pow(saturate(1-r*r), 2.0) * cloud;
""")
lib.connect_material_expressions(uv, "", mask, "UV")
opacity = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
opacity.set_editor_property("parameter_name", "DustOpacity")
opacity.set_editor_property("default_value", 0.0)
mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply)
lib.connect_material_expressions(mask, "", mul, "A")
lib.connect_material_expressions(opacity, "", mul, "B")
lib.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)
color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
color.set_editor_property("parameter_name", "DustColor")
color.set_editor_property("default_value", unreal.LinearColor(0.30, 0.19, 0.085, 1.0))
lib.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
print("Saved Anjanath dust material", mat.get_path_name())
