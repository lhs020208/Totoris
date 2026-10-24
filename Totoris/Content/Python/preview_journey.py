"""PIE-only visual review. Freeze around the chirp; stop PIE to restore normal time."""
import unreal
import sys
from pathlib import Path

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
pc.call_method('StartEventPreview', ('Journey',))
started = unreal.GameplayStatics.get_time_seconds(world)
freeze_seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 2.2

def freeze_journey(delta):
    if unreal.GameplayStatics.get_time_seconds(world) - started >= freeze_seconds:
        unreal.GameplayStatics.set_global_time_dilation(world, .0001)
        unreal.unregister_slate_post_tick_callback(journey_preview_handle)
        Path(unreal.Paths.project_saved_dir(), 'journey_preview_ready.txt').write_text('Frozen at %.2fs for visual review' % freeze_seconds)

journey_preview_handle = unreal.register_slate_post_tick_callback(freeze_journey)
