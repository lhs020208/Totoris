"""PIE-only close-up of burning Asito; stopping PIE discards camera changes."""
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
camera = unreal.GameplayStatics.get_player_controller(world, 0).get_view_target()
camera.set_actor_transform(unreal.Transform(location=unreal.Vector(-48210, -14700, 190), rotation=unreal.Rotator(pitch=-10, yaw=25, roll=0)), False, False)
unreal.WidgetLayoutLibrary.remove_all_widgets(world)
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
event = next(c for a in actors if (c := a.get_component_by_class(unreal.MinecraftBlockEventComponent)))
event.call_method("PlayEvent")
