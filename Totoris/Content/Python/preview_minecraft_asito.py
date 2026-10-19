"""Temporary PIE-only close-up. Stop PIE to discard the camera/UI changes."""
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
unreal.GameplayStatics.set_global_time_dilation(world, 0.1)
transform = unreal.Transform(location=unreal.Vector(-48487, -14891, 350), rotation=unreal.Rotator(pitch=8, yaw=12, roll=0))
camera = unreal.GameplayStatics.get_player_controller(world, 0).get_view_target()
camera.set_actor_transform(transform, False, False)
unreal.WidgetLayoutLibrary.remove_all_widgets(world)
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
event = next(c for a in actors if (c := a.get_component_by_class(unreal.MinecraftBlockEventComponent)))
event.call_method("PlayEvent")
