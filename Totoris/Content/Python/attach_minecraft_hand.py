import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
source = next(a for a in actors if a.get_actor_label() == "MinecraftBlock")
held = next(a for a in actors if a.get_actor_label() == "MinecraftBlock2")
scale = held.get_actor_scale3d()
helper = unreal.new_object(unreal.MinecraftBlockEventComponent, outer=source)
assert helper.call_method("PrepareHeldBlock"), "Hand socket preparation failed"
assert (held.get_actor_scale3d() - scale).length() < .001
source.modify()
tags = list(source.tags)
if "MinecraftBlock" not in [str(t) for t in tags]:
    tags.append("MinecraftBlock")
source.set_editor_property("tags", tags)
assert unreal.EditorAssetLibrary.save_asset("/Game/Totoris/Objects/Minecraft/MC_CustomSkin_Model")
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Minecraft hand prepared: " + str(held.get_actor_transform()))
