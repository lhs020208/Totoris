"""PIE-only smoke test for SC setup/list navigation; never starts a game."""
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc = unreal.GameplayStatics.get_player_controller(world, 0)
manager = unreal.find_object(None, pc.get_path_name() + '.TotorisMenuManager_0')
assert manager

def screen(prefix):
    return next(o for o in unreal.ObjectIterator(unreal.UserWidget)
                if o and o.get_name().startswith(prefix + '_C_')
                and 'GameInstance_' in o.get_path_name() and o.get_owning_player() == pc)

manager.call_method('HandleChallengeRequested')
setup = screen('WBP_ModeSetup')
assert setup.get_editor_property('ModeSpecificSwitcher').get_active_widget_index() == 2
manager.call_method('HandleSCListRequested')
sc = screen('WBP_SCList')
checks = [sc.get_editor_property('FCB' + str(i)) for i in range(1, 11)]
assert sc.is_in_viewport() and all(c.is_checked() for c in checks)
checks[0].set_is_checked(False)
manager.call_method('HandleMissionListBackRequested')
assert setup.is_in_viewport() and not sc.is_in_viewport()
assert setup.get_editor_property('ModeSpecificSwitcher').get_active_widget_index() == 2
manager.call_method('HandleSCListRequested')
assert screen('WBP_SCList') == sc and not checks[0].is_checked()
for c in checks:
    c.set_is_checked(False)
manager.call_method('HandleMissionListBackRequested')
assert sc.is_in_viewport(), 'Empty selection should not save'
manager.call_method('HandleMissionWarningDismissRequested')
checks[0].set_is_checked(True)
manager.call_method('HandleMissionListBackRequested')
assert setup.is_in_viewport() and not sc.is_in_viewport(), 'One SC mission must allow saving'
manager.call_method('HandleSCListRequested')
assert checks[0].is_checked() and sum(c.is_checked() for c in checks) == 1
manager.call_method('HandleMissionListRestoreRequested')
assert all(c.is_checked() for c in checks)
manager.call_method('HandleMissionListBackRequested')
pc.show_mode_setup(unreal.TotorisGameSetupMode.MISSION)
assert setup.get_editor_property('ModeSpecificSwitcher').get_active_widget_index() == 1
pc.show_mode_setup(unreal.TotorisGameSetupMode.CLASSIC)
assert setup.get_editor_property('ModeSpecificSwitcher').get_active_widget_index() == 0
manager.call_method('HandleChallengeRequested')
manager.call_method('HandleSCListRequested')
unreal.log('SC_LIST_TEST_PASS: panels, navigation, saved selection, minimum selection, all-on')
