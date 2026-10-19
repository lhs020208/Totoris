"""PIE-only SC run setup/completion test; does not implement mission evaluation.

Run setup(), then finish() from the editor console after inspecting the HUD.
"""
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
manager = unreal.find_object(None, pc.get_path_name() + '.TotorisMenuManager_0')

def setup():
    manager.call_method('HandleChallengeRequested')
    manager.call_method('HandleSCListRequested')
    sc = next(o for o in unreal.ObjectIterator(unreal.UserWidget)
              if o and o.get_name().startswith('WBP_SCList_C_')
              and 'GameInstance_' in o.get_path_name() and o.get_owning_player() == pc)
    for i in range(1, 11):
        sc.get_editor_property('FCB' + str(i)).set_is_checked(i in (1, 6, 10))
    manager.call_method('HandleMissionListBackRequested')
    pc.start_classic_game()
    unreal.log('SC_RUN_TEST_STARTED: selected 1,6,10 only')

def finish():
    # Identity-checked notifications simulate future evaluators. Out-of-order
    # and duplicate notifications must be ignored, never complete another ID.
    ids = ['S_Quad_Only_I', 'S_All_Spins_B2B', 'S_Unique_Spin_Centers']
    pc.call_method('NotifySChallengeMissionCompleted', ('S_Build_Heart',))
    for _ in range(3):
        for mission_id in ids:
            pc.call_method('NotifySChallengeMissionCompleted', (mission_id,))
    generator = next(c for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                     if (c := a.get_component_by_class(unreal.TotorisBlockGeneratorComponent)))
    summary = generator.get_last_finished_run_summary()
    assert summary.valid and summary.result == unreal.TotorisRunResult.COMPLETED
    unreal.log('SC_RUN_TEST_COMPLETED: selected queue exhausted through standard completed result')

setup()
