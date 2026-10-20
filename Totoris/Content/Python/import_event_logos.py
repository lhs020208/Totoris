"""Import the event gallery's source logos as UI textures."""
import unreal
from pathlib import Path

source = Path(unreal.Paths.project_dir()) / "Image" / "Logos"
names = ["Smilemo", "SANABI", "Higurashi_Onikakushi", "Palworld",
         "MonsterHunter", "MahjongSoul", "REANIMAL", "Minecraft"]
tasks = []
for name in names:
    assert (source / (name + ".png")).is_file(), name
    task = unreal.AssetImportTask()
    task.filename = str(source / (name + ".png"))
    task.destination_path = "/Game/Totoris/UI/Logos"
    task.destination_name = name
    task.automated = True
    task.replace_existing = False
    task.save = True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in names:
    texture = unreal.load_asset("/Game/Totoris/UI/Logos/" + name)
    assert isinstance(texture, unreal.Texture2D), name
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
for name in ["WBP_Event", "WBP_EventButton"]:
    unreal.EditorAssetLibrary.save_asset("/Game/Totoris/UI/Widgets/" + name)
unreal.log("EVENT_LOGOS_IMPORTED: 8")
