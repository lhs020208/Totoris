"""PIE gallery smoke test: Journey logo ninth slot, wraparound, execution, replay and cleanup.

Run in an idle PIE session. Never enters mission mode. Wait for JOURNEY_TEST_PASS.
"""
import unreal
from pathlib import Path

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
manager = unreal.find_object(None, pc.get_path_name() + '.TotorisMenuManager_0')
assert manager
unreal.GameplayStatics.set_global_time_dilation(world, 1.0)

def gallery():
    return next(o for o in unreal.ObjectIterator(unreal.UserWidget)
                if o.get_name().startswith('WBP_Event_C_') and o.get_owning_player() == pc and o.is_in_viewport())

def logo():
    widget = gallery()
    return next(o for o in unreal.ObjectIterator(unreal.Image)
                if o.get_name() == 'EventLogo' and o.get_path_name().startswith(widget.get_path_name()+'.'))

def hosts():
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if any(c.static_mesh and c.static_mesh.get_name() == 'SM_JourneyTraveler'
                   for c in a.get_components_by_class(unreal.StaticMeshComponent))]

def journey_audio():
    return [a for a in unreal.ObjectIterator(unreal.AudioComponent)
            if a.get_world() == world and a.sound and a.sound.get_name().startswith('SW_Journey')
            and a.is_playing()]

manager.call_method('HandleEventRequested')
expected = ['Smilemo','SANABI','Higurashi_Onikakushi','Palworld','MonsterHunter','MahjongSoul','REANIMAL','Minecraft']
for name in expected:
    assert logo().get_editor_property('brush').get_editor_property('resource_object').get_name() == name
    manager.call_method('HandleEventNext')
assert logo().get_editor_property('brush').get_editor_property('resource_object').get_name() == 'Journey'
assert logo().get_visibility() == unreal.SlateVisibility.VISIBLE
manager.call_method('HandleEventNext')
assert logo().get_editor_property('brush').get_editor_property('resource_object').get_name() == 'Smilemo'
manager.call_method('HandleEventPrevious')
assert logo().get_editor_property('brush').get_editor_property('resource_object').get_name() == 'Journey'
manager.call_method('HandleEventStart')
assert len(hosts()) == 1, 'Blank Journey slot must start its own presentation'
assert any(a.sound.get_name() == 'SW_JourneyWind' for a in journey_audio()), 'Wind must start with presentation'
started = unreal.GameplayStatics.get_time_seconds(world)
phase = 0
audio_checked = False
ascent_checked = False

def check_journey(delta):
    global phase, started, audio_checked, ascent_checked
    try:
        age = unreal.GameplayStatics.get_time_seconds(world) - started
        if phase == 0 and 4.0 < age < 8.0 and not audio_checked:
            names = [a.sound.get_name() for a in hosts()[0].get_components_by_class(unreal.AudioComponent)]
            assert sorted(names) == sorted(['SW_Journey' + n for n in
                ['Wind', 'Arrival', 'Chirp', 'ClothGather', 'ScarfCharge', 'FlightWhoosh']]), names
            audio_checked = True
        if phase == 0 and 8.5 < age < 10.5 and not ascent_checked:
            names = [a.sound.get_name() for a in hosts()[0].get_components_by_class(unreal.AudioComponent)]
            assert names.count('SW_JourneyFlightWhoosh') == 2, names
            ascent_checked = True
        if phase == 0 and age > 11.5:
            assert audio_checked and ascent_checked, 'Audio cue stages must be observed'
            assert not hosts(), 'Presentation must be destroyed on natural completion'
            assert not journey_audio(), 'Natural completion must stop all Journey sounds'
            assert logo().get_editor_property('brush').get_editor_property('resource_object').get_name() == 'Journey', 'Return to selected Journey slot'
            manager.call_method('HandleEventStart')
            assert len(hosts()) == 1, 'Replay must create exactly one presentation'
            started = unreal.GameplayStatics.get_time_seconds(world)
            phase = 1
        elif phase == 1 and age > 1.0:
            manager.call_method('HandleEventBack')
            phase = 2
        elif phase == 2:
            assert not hosts(), 'Leaving preview must clean up all event meshes'
            assert not journey_audio(), 'Cancel must stop all Journey sounds'
            manager.call_method('HandleEventRequested')
            for _ in range(8): manager.call_method('HandleEventNext')
            assert logo().get_editor_property('brush').get_editor_property('resource_object').get_name() == 'Journey'
            unreal.unregister_slate_post_tick_callback(journey_test_handle)
            Path(unreal.Paths.project_saved_dir(), 'journey_test_result.txt').write_text('PASS: order, blank logo, wraparound, start, finish, replay, cancel cleanup')
            unreal.log('JOURNEY_TEST_PASS: gallery, audio cues, ascent, finish, replay, audio cancel cleanup')
    except Exception:
        unreal.unregister_slate_post_tick_callback(journey_test_handle)
        raise

journey_test_handle = unreal.register_slate_post_tick_callback(check_journey)

