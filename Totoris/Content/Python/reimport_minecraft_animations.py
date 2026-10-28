"""Rebuild Minecraft animation tracks/compression against the final mesh skeleton."""
import unreal

mesh = unreal.load_asset('/Game/Totoris/Objects/Minecraft/MC_CustomSkin_Model')
assert mesh and mesh.skeleton
for name in ['MC_CrouchPose', 'MC_LookDown', 'MC_PlaceBlock', 'MC_Walk']:
    task = unreal.AssetImportTask()
    task.filename = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + 'Blender/Asito_Minecraft/' + name + '.fbx')
    task.destination_path = '/Game/Totoris/Objects/Minecraft'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.skeleton = mesh.skeleton
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    clip = unreal.load_asset(task.destination_path + '/' + name)
    assert clip and clip.get_editor_property('skeleton') == mesh.skeleton, name
    assert unreal.EditorAssetLibrary.save_loaded_asset(clip, False)
unreal.log('MINECRAFT_ANIMATIONS_REIMPORTED')
