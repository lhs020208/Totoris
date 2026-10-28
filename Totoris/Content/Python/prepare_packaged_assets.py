"""Persist runtime actor identities and animation compatibility before cooking."""
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/L_Totoris')
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
required = {
    'SmilemoSito': 'SmilemoSito', 'SmilemoBoss': 'SmilemoBoss',
    'Dragon_Model': 'DragonFlightActor', 'Plane2': 'DragonWindowTarget',
    'Anjanath_Model': 'AnjanathEventActor', 'MinecraftBlock': 'MinecraftBlock',
    'MinecraftBlock2': 'MinecraftHeldBlock', 'MC_CustomSkin_Model': 'MinecraftAsito',
    'Scene_RoundFloor': 'Scene_RoundFloor',
}
for label, tag in required.items():
    matches = [a for a in actors if a.get_actor_label() == label]
    assert len(matches) == 1, (label, len(matches))
    actor = matches[0]
    actor.modify()
    actor.set_editor_property('tags', list(dict.fromkeys([str(t) for t in actor.tags] + [tag])))
    actor.set_editor_property('is_spatially_loaded', False)
    if label not in ['Plane2', 'Scene_RoundFloor']:
        actor.set_actor_hidden_in_game(True)
    unreal.log('PACKAGING_ACTOR_FIXED ' + label)

mesh = unreal.load_asset('/Game/Totoris/Anim/Totori_MotionReady')
task = unreal.AssetImportTask()
task.filename = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + 'Blender/Totori/Totori_idle_neutral.fbx')
task.destination_path = '/Game/Totoris/Anim'
task.destination_name = 'Totori_Idle_Runtime'
task.automated = True
task.replace_existing = True
task.save = True
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
options.import_mesh = False
options.import_animations = True
options.skeleton = mesh.skeleton
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
idle = unreal.load_asset('/Game/Totoris/Anim/Totori_Idle_Runtime')
assert idle and idle.get_editor_property('skeleton') == mesh.skeleton
for actor in actors:
    if isinstance(actor, unreal.TotoriAICharacter) and actor.get_component_by_class(unreal.SkeletalMeshComponent).get_skeletal_mesh_asset() == mesh:
        actor.modify()
        actor.set_editor_property('idle_animation', idle)
bp = unreal.load_asset('/Game/Totoris/Blueprints/BP_TotoriAI')
if bp:
    unreal.get_default_object(bp.generated_class()).set_editor_property('idle_animation', idle)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
assert unreal.EditorAssetLibrary.save_loaded_asset(idle, False)
mc_mesh = unreal.load_asset('/Game/Totoris/Objects/Minecraft/MC_CustomSkin_Model')
unreal.SkeletalMeshEditorSubsystem.set_allow_cpu_access(mc_mesh, True)
assert mc_mesh.find_socket('MinecraftRightHandTip'), 'Missing hand socket'
for name in ['MC_CrouchPose', 'MC_LookDown', 'MC_PlaceBlock', 'MC_Walk']:
    clip = unreal.load_asset('/Game/Totoris/Objects/Minecraft/' + name)
    assert clip and clip.get_editor_property('skeleton') == mc_mesh.skeleton, name
assert unreal.EditorAssetLibrary.save_loaded_asset(mc_mesh, False)
assert level.save_current_level()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('PACKAGING_ASSETS_SAVED')
