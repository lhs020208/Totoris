"""PIE-only smoke test: start the bridge event without changing mission state."""
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "Run PIE before this smoke test"
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
events = [a.get_component_by_class(unreal.MinecraftBlockEventComponent) for a in actors]
events = [c for c in events if c]
assert len(events) == 1, "Expected the controller's single Minecraft event component"
event = events[0]
event.call_method("StopEvent")
model = next(a for a in actors if a.get_actor_label() == "MC_CustomSkin_Model")
baseline = model.get_actor_transform()
original_mesh = model.get_component_by_class(unreal.SkeletalMeshComponent)
held = next(a for a in actors if a.get_actor_label() == "MinecraftBlock2")
held_scale = held.get_actor_scale3d()
original_blocks = {a.get_name() for a in actors if a.get_class() == event.get_owner().get_class()}
rotations = []
unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
event.call_method("PlayEvent")
start = unreal.GameplayStatics.get_time_seconds(world)
samples = []
errors = []

def verify_tick(_delta):
    now = unreal.GameplayStatics.get_time_seconds(world) - start
    p = model.get_actor_location()
    samples.append((now, p.x, p.y, p.z))
    pose = model.get_component_by_class(unreal.PoseableMeshComponent)
    if pose:
        if held.root_component.get_attach_parent() != pose:
            errors.append("Held block not attached to animated mesh")
        if (held.get_actor_location() - pose.get_socket_location("MinecraftRightHandTip")).length() > .01:
            errors.append("Held block not following hand tip")
        if (held.get_actor_scale3d() - held_scale).length() > .001:
            errors.append("Held block scale changed")
        r = held.get_actor_rotation()
        rotations.append((r.pitch, r.yaw, r.roll))
    if abs(p.x - baseline.translation.x) > 0.01:
        errors.append("X changed during the event")
    if now < 7.5:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    moving = [(a, b) for a, b in zip(samples, samples[1:]) if .1 < a[0] < 2.5 and b[0] - a[0] > .001]
    speeds = [(b[2] - a[2]) / (b[0] - a[0]) for a, b in moving]
    if not speeds or max(abs(v - 100) for v in speeds) > 1:
        errors.append("Backward speed differs from 100 cm/s")
    late = [s for s in samples if 5.0 < s[0] < 6.0]
    if len(late) < 2 or max(s[2] for s in late) - min(s[2] for s in late) > .01:
        errors.append("Horizontal movement continued during fall")
    if len(late) < 2 or max(s[3] for s in late) - min(s[3] for s in late) > .01:
        errors.append("Did not settle on the floor")
    if not late or late[-1][3] >= samples[0][3] - 100:
        errors.append("Did not descend to the floor")
    pose_meshes = model.get_components_by_class(unreal.PoseableMeshComponent)
    if len(pose_meshes) != 1 or not pose_meshes[0].is_visible():
        errors.append("Landed model disappeared")
    copies = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, event.get_owner().get_class()) if a.get_name() not in original_blocks]
    if len(copies) != 3:
        errors.append("Expected exactly three runtime blocks: " + str(len(copies)))
    event.call_method("StopEvent")
    if held.root_component.get_attach_parent() != original_mesh:
        errors.append("Held block not restored to original mesh")
    if not rotations or max(r[0] for r in rotations) - min(r[0] for r in rotations) < 5:
        errors.append("Held block did not rotate with the arm")
    if (model.get_actor_location() - baseline.translation).length() > .01:
        errors.append("Stop did not restore authored position")
    if model.get_components_by_class(unreal.PoseableMeshComponent):
        errors.append("Stop left an event pose mesh")
    # A second start before garbage collection specifically exercises the old
    # duplicate-object-name crash and the new original-mesh restoration path.
    event.call_method("PlayEvent")
    event.call_method("StopEvent")
    if held.root_component.get_attach_parent() != original_mesh:
        errors.append("Restart lost hand attachment")
    if errors:
        unreal.log_error("Minecraft smoke test FAILED: " + str(errors))
    else:
        unreal.log("Minecraft smoke test PASSED: 100cm/s, three blocks, floor landing, visible idle, stop/restart restore")

handle = unreal.register_slate_post_tick_callback(verify_tick)
unreal.log("Minecraft smoke test started (7.5 game seconds)")
