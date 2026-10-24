"""Import the project's named Journey WAV sources, with PIE stopped."""
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
source = Path(unreal.Paths.project_dir()) / 'Audio' / 'Journey'
tasks = []
for filename in sorted(source.glob('SW_Journey*.wav')):
    task = unreal.AssetImportTask()
    task.filename = str(filename.resolve())
    task.destination_path = '/Game/Totoris/Audio/Journey'
    task.destination_name = filename.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)
assert len(tasks) == 6
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    asset = unreal.load_asset(task.destination_path + '/' + task.destination_name)
    assert isinstance(asset, unreal.SoundWave), task.filename
    assert asset.get_editor_property('duration') > 0
    if asset.get_name() == 'SW_JourneyWind':
        asset.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log('JOURNEY_AUDIO_IMPORTED: %s %.3fs' % (asset.get_name(), asset.get_editor_property('duration')))
unreal.log('JOURNEY_AUDIO_IMPORT_PASS: 6 SoundWaves')
