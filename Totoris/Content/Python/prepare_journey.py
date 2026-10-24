"""Import the unrigged traveler and build the gallery event's procedural materials."""
import unreal
from pathlib import Path

ROOT = '/Game/Totoris/Objects/Journey'
assets = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
source = Path(unreal.Paths.project_dir()) / 'Blender/Journey/red cloaked figure 3d model'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before rebuilding materials'

def import_asset(filename, name, options=None):
    existing = unreal.load_asset(ROOT + '/' + name)
    if existing:
        return existing
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = ROOT
    task.destination_name = name
    task.automated = True
    task.save = True
    if options:
        task.options = options
    assets.import_asset_tasks([task])
    result = unreal.load_asset(ROOT + '/' + name)
    assert result, (name, task.imported_object_paths)
    return result

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.import_animations = False
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.generate_lightmap_u_vs = False
mesh = import_asset(source / 'red_cloaked_figure_3d_model.fbx', 'SM_JourneyTraveler', options)
texture = import_asset(source / 'red_cloaked_figure_3d_model.fbm/red_cloaked_figure_3d_model_basecolor.jpg', 'T_JourneyTraveler')

def expr(mat, cls, **props):
    node = lib.create_material_expression(mat, cls)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node

def custom(mat, code, inputs, output=unreal.CustomMaterialOutputType.CMOT_FLOAT1):
    node = expr(mat, unreal.MaterialExpressionCustom, code=code, output_type=output)
    entries = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        entries.append(entry)
    node.set_editor_property('inputs', entries)
    for name, (src, pin) in inputs.items():
        assert lib.connect_material_expressions(src, pin, node, name)
    return node

def add_robe_motion(mat):
    if lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        return
    position = expr(mat, unreal.MaterialExpressionPreSkinnedPosition)
    clock = expr(mat, unreal.MaterialExpressionScalarParameter, parameter_name='EventTime', default_value=0.0)
    flight = expr(mat, unreal.MaterialExpressionScalarParameter, parameter_name='Flight', default_value=0.0)
    offset = custom(mat, '''float hem=pow(saturate(1-P.z/78.0),2.0);
float angle=atan2(P.y,P.x);
float ripple=sin(angle*3.0+P.z*.13-T*3.8);
float flutter=sin(angle*5.0-P.z*.10+T*5.1);
float2 radial=normalize(P.xy+float2(.001,.001));
float2 bend=radial*(ripple*2.5+flutter*1.2)*hem;
bend.y-=hem*(3.0+Flight*6.0+sin(T*2.4-P.z*.09)*2.0);
return float3(bend,hem*(ripple*1.7+flutter*.6));''',
                    {'P':(position,''),'T':(clock,''),'Flight':(flight,'')}, unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    transform = expr(mat, unreal.MaterialExpressionTransform,
                     transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                     transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    assert lib.connect_material_expressions(offset, '', transform, lib.get_material_expression_input_names(transform)[0])
    assert lib.connect_material_property(transform, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

for name, kind in [('M_JourneyTraveler', 'traveler'), ('M_JourneyCloth', 'cloth'), ('M_JourneyGold', 'gold')]:
    # Fade only. Depth comes from the actual scene, never a rectangular UI cutout.
    mask_code = '''float2 pixel=floor(S*View.ViewSizeAndInvSize.xy);
float noise=frac(sin(dot(pixel,float2(12.9898,78.233)))*43758.5453);
return step(noise+0.001,Fade);'''
    mat = unreal.load_asset(ROOT + '/' + name)
    if mat:
        # Native constructor asset references can root expressions. Never delete
        # a loaded graph; upgrade the emissive output in place instead.
        previous = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        assert previous, 'Existing material has no emissive source: ' + name
        if not isinstance(previous, unreal.MaterialExpressionEyeAdaptationInverse):
            exposure = expr(mat, unreal.MaterialExpressionEyeAdaptationInverse)
            pin = 'RGB' if isinstance(previous, unreal.MaterialExpressionTextureSample) else ''
            assert lib.connect_material_expressions(previous, pin, exposure, lib.get_material_expression_input_names(exposure)[0])
            assert lib.connect_material_property(exposure, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mask = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY_MASK)
        if not mask:
            mask = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY)
        assert isinstance(mask, unreal.MaterialExpressionCustom)
        mask.set_editor_property('code', mask_code)
        assert lib.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property('two_sided', kind != 'traveler')
        if kind == 'traveler':
            add_robe_motion(mat)
        lib.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
        continue
    mat = assets.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', kind != 'traveler')
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate)
    fade = expr(mat, unreal.MaterialExpressionScalarParameter, parameter_name='Fade', default_value=1.0)
    charge = expr(mat, unreal.MaterialExpressionScalarParameter, parameter_name='Charge', default_value=0.0)
    rect = expr(mat, unreal.MaterialExpressionVectorParameter, parameter_name='BoardRect', default_value=unreal.LinearColor(0,0,0,0))
    screen = expr(mat, unreal.MaterialExpressionScreenPosition)
    opacity = custom(mat, mask_code,
                     {'S':(screen,'ViewportUV'), 'Rect':(rect,'RGBA'), 'Fade':(fade,'')})
    assert lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    if kind == 'traveler':
        sample = expr(mat, unreal.MaterialExpressionTextureSample, texture=texture)
        color = sample
        pin = 'RGB'
    elif kind == 'cloth':
        color = custom(mat, '''float2 q=float2(frac(UV.x*9.0), UV.y);
float border=step(q.y,.07)+step(.93,q.y);
float2 d=abs(q-float2(.5,.5));
float box=step(d.x,.24)*step(d.y,.28)*(1-step(d.x,.14)*step(d.y,.18));
float bar=step(d.x,.055)*step(d.y,.34);
float glyph=saturate(border+box+bar);
return lerp(float3(.38,.012,.018),float3(.85,.40,.065)*(1+Charge*.65),glyph);''',
                       {'UV':(uv,''),'Charge':(charge,'')}, unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        pin = ''
    else:
        color = expr(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0,.65,.19,1))
        pin = ''
    exposure = expr(mat, unreal.MaterialExpressionEyeAdaptationInverse)
    assert lib.connect_material_expressions(color, pin, exposure, lib.get_material_expression_input_names(exposure)[0])
    assert lib.connect_material_property(exposure, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if kind == 'traveler':
        add_robe_motion(mat)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

mesh.set_material(0, unreal.load_asset(ROOT + '/M_JourneyTraveler'))
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
report = str(mesh.get_bounds())
Path(unreal.Paths.project_saved_dir(), 'journey_assets.txt').write_text(report)
unreal.log('JOURNEY_ASSETS_READY ' + report)
