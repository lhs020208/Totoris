"""PIE-only close-up of the first block's fragments."""
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
camera = unreal.GameplayStatics.get_player_controller(world, 0).get_view_target()
camera.set_actor_transform(unreal.Transform(location=unreal.Vector(-48300, -14900, 500), rotation=unreal.Rotator(pitch=-15, yaw=30, roll=0)), False, False)
unreal.WidgetLayoutLibrary.remove_all_widgets(world)
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
event = next(c for a in actors if (c := a.get_component_by_class(unreal.MinecraftBlockEventComponent)))
event.call_method("PlayEvent")

def slow_break(_delta):
    debris = event.get_owner().get_component_by_class(unreal.InstancedStaticMeshComponent)
    if debris and debris.get_instance_count() > 0:
        unreal.GameplayStatics.set_global_time_dilation(world, .005)
        unreal.unregister_slate_post_tick_callback(handle)

handle = unreal.register_slate_post_tick_callback(slow_break)
