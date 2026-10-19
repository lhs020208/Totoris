import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
event = next(c for a in actors if (c := a.get_component_by_class(unreal.MinecraftBlockEventComponent)))
model = next(a for a in actors if a.get_actor_label() == "MC_CustomSkin_Model")
event.call_method("PlayEvent")
lava_class = unreal.load_class(None, "/Script/Totoris.MinecraftLavaComponent")
event.get_owner().get_component_by_class(lava_class).call_method("StopLava")
start = unreal.GameplayStatics.get_time_seconds(world)

def check(_delta):
    if unreal.GameplayStatics.get_time_seconds(world) - start < 6:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    burning = bool(model.get_components_by_class(unreal.DynamicMeshComponent))
    event.call_method("StopEvent")
    assert not burning, "Unexpected fire without lava"
    unreal.log("Minecraft fire negative test PASSED: no ignition without lava")

handle = unreal.register_slate_post_tick_callback(check)
