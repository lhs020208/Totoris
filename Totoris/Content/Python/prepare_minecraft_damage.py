import unreal

assets = unreal.AssetToolsHelpers.get_asset_tools()
for filename, name in (("PlaceNetherrack/break1.ogg", "MinecraftPlaceSound"), ("PlayerFireHurt/fire_hurt1.ogg", "MinecraftFireHurtSound")):
    path = "/Game/Totoris/Audio/Gameplay/" + name
    if not unreal.load_asset(path):
        task = unreal.AssetImportTask()
        task.filename = "D:/GitHub/Totoris/Totoris/AudioSource/Minecraft/" + filename
        task.destination_path = "/Game/Totoris/Audio/Gameplay"
        task.destination_name = name
        task.automated = True
        task.save = True
        assets.import_asset_tasks([task])
    assert isinstance(unreal.load_asset(path), unreal.SoundWave), path

lib = unreal.MaterialEditingLibrary
path = "/Game/Totoris/Objects/Materials/Minecraft/M_MinecraftHurt"
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.EditorAssetLibrary.duplicate_asset("/Game/Totoris/Objects/Minecraft/M_MC_Skin", path)
    source = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    red = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    red.set_editor_property("constant", unreal.LinearColor(1.0, .015, .015, 1.0))
    strength = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
    strength.set_editor_property("parameter_name", "HurtAlpha")
    strength.set_editor_property("default_value", .65)
    lerp = lib.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate)
    assert lib.connect_material_expressions(source, "RGB", lerp, "A")
    assert lib.connect_material_expressions(red, "", lerp, "B")
    assert lib.connect_material_expressions(strength, "", lerp, "Alpha")
    assert lib.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log("Minecraft damage assets ready")
