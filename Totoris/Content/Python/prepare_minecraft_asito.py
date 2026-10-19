"""Import the four bone-layered event clips without replacing the skin mesh."""
import unreal

folder = "/Game/Totoris/Objects/Minecraft"
source = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + "Blender/Asito_Minecraft/"
skeleton = unreal.load_asset(folder + "/MC_CustomSkin_Model_Skeleton")
assert skeleton, "Import the Minecraft model first"
for name in ("MC_LookDown", "MC_PlaceBlock", "MC_CrouchPose", "MC_Walk"):
    path = folder + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        options = unreal.FbxImportUI()
        options.set_editor_property("automated_import_should_detect_type", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        options.set_editor_property("import_mesh", False)
        options.set_editor_property("import_animations", True)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("skeleton", skeleton)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source + name + ".fbx")
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", options)
        task.set_editor_property("factory", unreal.FbxFactory())
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        unreal.log("Minecraft clip imported: " + str(task.imported_object_paths))
    clip = unreal.load_asset(path)
    assert isinstance(clip, unreal.AnimSequence), "Missing animation: " + path
    unreal.log("Minecraft clip ready: " + path)

mesh = unreal.load_asset(folder + "/MC_CustomSkin_Model")
unreal.SkeletalMeshEditorSubsystem.set_allow_cpu_access(mesh, True)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
models = [a for a in actors if a.get_actor_label() == "MC_CustomSkin_Model"]
blocks = [a for a in actors if a.get_actor_label() == "MinecraftBlock"]
if len(models) == 1 and len(blocks) == 1:
    model, block = models[0], blocks[0]
    model_origin, model_extent = model.get_actor_bounds(False)
    block_origin, block_extent = block.get_actor_bounds(False)
    location = model.get_actor_location()
    location.z += block_origin.z + block_extent.z - (model_origin.z - model_extent.z)
    with unreal.ScopedEditorTransaction("Align Minecraft Asito feet to block"):
        model.modify()
        model.set_actor_location(location, False, False)
    unreal.log("Minecraft editor feet alignment: " + str(location))
    # Do not save the whole map here: it may contain other unsaved user edits.
unreal.log("Minecraft animation preparation complete (foot geometry CPU access enabled)")
