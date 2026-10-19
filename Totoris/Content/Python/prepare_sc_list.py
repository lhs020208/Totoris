"""Populate the authored S Challenge list from the project's mission catalogue.

Run in the Unreal Editor; gameplay is intentionally not implemented here.
"""
from pathlib import Path
import unreal

path = '/Game/Totoris/UI/Widgets/WBP_SCList'
bp = unreal.load_asset(path)
assert bp

def widget(name):
    obj = unreal.find_object(None, path + '.WBP_SCList:WidgetTree.' + name)
    assert obj, name
    return obj

catalogue = Path(unreal.Paths.project_dir()).parent / 'totoris_missions.txt'
missions = [line.split(' / ', 2)[1:] for line in catalogue.read_text(encoding='utf-8-sig').splitlines()
            if line.startswith('S / ')]
assert len(missions) == 10
font = unreal.load_asset('/Game/Slate/Fonts/NotoSansCJKkr-Medium_Font')
assert font
with unreal.ScopedEditorTransaction('Populate S Challenge mission list'):
    bp.modify()
    widget('TitleText').set_text('S Challenge List')
    # Keep the existing two-column, five-row design. Increase row spacing to
    # use the free space formerly reserved for a sixth normal-mission row.
    for index, (english, korean) in enumerate(missions, 1):
        text = widget('FText' + str(index))
        check = widget('FCB' + str(index))
        text.modify()
        check.modify()
        display = korean
        if index == 6:
            # Use the same inline-mino renderer as the normal mission list.
            display = display.replace('I/J/L/S/Z/T', '/'.join(
                '<mino id="' + piece + '"/>' for piece in 'IJLSZT'))
            text.set_editor_property('decorator_classes', [unreal.load_class(
                None, '/Script/Totoris.TotorisMinoRichTextDecorator')])
        text.set_text(str(index) + '. ' + display)
        text.set_tool_tip_text(english + '\n' + korean)
        style = text.get_editor_property('default_text_style_override')
        style.font = unreal.SlateFontInfo(font_object=font, size=12)
        style.color_and_opacity = unreal.SlateColor(specified_color=unreal.LinearColor(1, 1, 1, 1))
        text.set_default_text_style(style)
        text.set_auto_wrap_text(True)
        text.set_editor_property('wrap_text_at', 260)
        text.slot.set_auto_size(False)
        column, row = divmod(index - 1, 5)
        text.slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0, 0), maximum=unreal.Vector2D(0, 0)))
        text.slot.set_alignment(unreal.Vector2D(0, 0))
        text.slot.set_position(unreal.Vector2D(16 + column * 330, 14 + row * 73))
        text.slot.set_size(unreal.Vector2D(260, 68))
        check.slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0, 0), maximum=unreal.Vector2D(0, 0)))
        check.slot.set_alignment(unreal.Vector2D(0, 0))
        check.slot.set_position(unreal.Vector2D(292 + column * 330, 25 + row * 73))
        check.slot.set_size(unreal.Vector2D(24, 24))
        check.set_is_checked(True)
        check.set_is_enabled(True)
    widget('SettingsSwitcher').set_active_widget_index(0)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
unreal.log('SC_LIST_PREPARED: 10 numbered missions; independent default-enabled checkboxes')
