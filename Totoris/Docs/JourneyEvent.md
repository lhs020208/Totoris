# Journey gallery preview

The ninth gallery slot, immediately after Minecraft, starts an 11-second
Journey vignette. Its transparent gold logo follows Minecraft. Journey is registered in
`TriggerRandomMissionSetCompletionEvent` and is selected on completion of a three-mission set.

The supplied FBX is imported as `SM_JourneyTraveler` under
`/Game/Totoris/Objects/Journey`. No skeleton or animation clips are required.
The traveler rises at the right, calls to five cloth creatures, glides left,
then ascends. A separate animated ribbon carries procedural gold glyphs, with a
chirp ring, diamond and sparse gold motes. All transient objects are destroyed on
completion, replay reset, cancellation and world teardown.

The presentation has one real scene depth in front of both the board and side
panels. A continuous high arch skirts the playfield instead of crossing its
center. There is no rectangular screen-space cutout or sudden depth switch.
Projection follows the active viewport rather than assuming a fixed aspect ratio.
The traveler leans into flight about the camera-forward axis, then straightens
into the ascent. Procedural material deformation ripples and bends the lower robe
while keeping the head stable. The scarf follows the rotated neck and trails the
recent trajectory, including downward during ascent. Both motions use event time,
so pause, replay and visual-review time dilation remain synchronized.

## Asset preparation

Run `Content/Python/prepare_journey.py` in the editor with PIE stopped. It imports
the FBX and base-color texture and creates the cloth, traveler and gold materials.
Existing material graphs are upgraded in place; do not delete rooted material
expressions referenced by native constructor assets. Unlit colors use inverse
eye adaptation so room exposure does not turn the red cloak black. Masked
materials avoid translucent self-sorting artifacts on the unrigged mesh.

## Review and validation

- Build `TotorisEditor Win64 Development`.
- In idle PIE, run `Content/Python/test_journey.py`. Wait for `JOURNEY_TEST_PASS`:
  it verifies all eight existing slots, the Journey logo ninth slot, wraparound, actual
  gallery start, natural completion, replay, cancellation cleanup and restoration
  of the selected Journey slot. It never starts a mission run.
- `Content/Python/preview_journey.py` freezes at the chirp for visual inspection.
  Stop PIE to discard this temporary time-dilation change.

The Journey gallery logo is imported as /Game/Totoris/UI/Logos/Journey.

## Audio

Named sources live in `Audio/Journey`; run `Content/Python/import_journey_audio.py`
with PIE stopped to import six SoundWaves into `/Game/Totoris/Audio/Journey`.
The chirp MP3 is retained as source and decoded to 48 kHz mono PCM WAV for import.

| Event time | Sound | Volume before master multiplier |
| --- | --- | --- |
| 0-11 s | Wind, fade in and out; louder in flight | 0.035-0.13 |
| 0.15 s | Arrival bell | 0.22 |
| 1.6 s | Traveler chirp, synchronized to visible ring | 0.65 |
| 2.15 s | Cloth creatures gather sparkle | 0.16 |
| 2.8 s | Scarf charge crystal chime | 0.24 |
| 3 s | Launch whoosh | 0.24 |
| 8 s | Ascent whoosh | 0.18 |

Audio components belong to the transient presentation. Completion, cancellation,
replay and world teardown stop and destroy them. Cues fire once using event time;
late frames seek into cues rather than delaying the whole sequence. Wind remains
virtualized while silent so its fade-in works. Existing master volume is respected.
Journey participates in the same idle-first random event selection as the other eight events.

Source mapping (user-downloaded files, 2026-10-22):
- `SW_JourneyArrival`: Mixkit relaxing-bell-chime-3109.
- `SW_JourneyClothGather`: Mixkit fairy-magic-sparkle-871.
- `SW_JourneyScarfCharge`: Mixkit crystal-chime-3108.
- `SW_JourneyFlightWhoosh`: Mixkit transition-windy-swoosh-1474.
- `SW_JourneyWind`: Mixkit wind-blowing-ambience-2658.
- `SW_JourneyChirp`: user-supplied `journey_ps3_chirp.mp3` (previously linked
  Journey PS3 Chirp on Zedge; reuse permission not established).

Mixkit SFX terms: https://mixkit.co/license/#sfxFree
Source pages: https://mixkit.co/free-sound-effects/wind/ ,
https://mixkit.co/free-sound-effects/chimes/ ,
https://mixkit.co/free-sound-effects/magic/

Logo source: https://commons.wikimedia.org/wiki/File:Logo_-_Journey.svg (thatgamecompany; Commons PD-textlogo). Original vector retained; gallery variant uses gold fill and transparent, tightly cropped PNG.

