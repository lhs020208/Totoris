"""PIE smoke test: two full death cycles, red flashes, hand/fire cleanup, reset."""
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
event = next(c for a in actors if (c := a.get_component_by_class(unreal.MinecraftBlockEventComponent)))
model = next(a for a in actors if a.get_actor_label() == "MC_CustomSkin_Model")
held = next(a for a in actors if a.get_actor_label() == "MinecraftBlock2")
event.call_method("StopEvent")
original_blocks = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, event.get_owner().get_class())}
baseline = model.get_actor_location()
unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
event.call_method("PlayEvent")
state = {"start": unreal.GameplayStatics.get_time_seconds(world), "cycle": 0, "red": False, "fire": False}

def check(_delta):
    now = unreal.GameplayStatics.get_time_seconds(world) - state["start"]
    pose = model.get_component_by_class(unreal.PoseableMeshComponent)
    assert pose
    if "M_MinecraftHurt" in pose.get_material(0).get_name():
        state["red"] = True
    if model.get_components_by_class(unreal.DynamicMeshComponent):
        state["fire"] = True
    if now < 12:
        return
    try:
        assert state["red"] and state["fire"], "Missing red flash/fire"
        assert not pose.is_visible(), "Dead model remained visible"
        assert held.get_editor_property("hidden"), "Held block remained visible"
        assert not model.get_components_by_class(unreal.DynamicMeshComponent), "Fire survived death"
        assert not event.get_owner().get_components_by_class(unreal.DynamicMeshComponent), "Lava survived event completion"
        assert not event.get_owner().get_components_by_class(unreal.InstancedStaticMeshComponent), "Fragments survived event completion"
        blocks = unreal.GameplayStatics.get_all_actors_of_class(world, event.get_owner().get_class())
        assert {a.get_name() for a in blocks} == original_blocks, "Generated blocks survived event completion"
        assert not event.is_component_tick_enabled(), "Event did not finish after lava disappeared"
        # Rotating the original upright up-axis around world Y by 90 degrees makes it horizontal.
        assert abs(pose.get_up_vector().z) < .001, "Death did not reach 90 degrees"
        event.call_method("StopEvent")
        assert not held.get_editor_property("hidden"), "Hand visibility not restored"
        assert (model.get_actor_location() - baseline).length() < .01
        assert not model.get_components_by_class(unreal.PoseableMeshComponent)
        state["cycle"] += 1
        if state["cycle"] == 2:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.log("Minecraft damage test PASSED: two full deaths, sequential block destruction, lava/fragment cleanup, completion and restart")
        else:
            state.update(start=unreal.GameplayStatics.get_time_seconds(world), red=False, fire=False)
            event.call_method("PlayEvent")
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle)
        raise

handle = unreal.register_slate_post_tick_callback(check)
