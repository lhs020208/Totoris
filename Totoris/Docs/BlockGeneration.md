# Block generation prototype

`L_Totoris` uses `UTotorisBlockGeneratorComponent` on the existing board actor.
The board, HOLD/NEXT panels, camera and background remain level assets.

- Each bag contains I, S, Z, T, O, L and J once, shuffled with Fisher–Yates.
- Starting play draws the active piece immediately. NEXT shows the following five,
  from top to bottom. HOLD remains empty.
- R resets the generation state in place, rotates the previous first bag one place
  left, and rebuilds the active piece and previews. Seven presses return to the
  original order. Starting a fresh PIE/game session randomizes the first bag anew.
- No gravity, movement, rotation, hold, collision or locking is implemented.

## Coordinates and shapes

Logical columns are zero-based; row 1 is the bottom row of the visible 10×20 board.
The highest occupied spawn row is **22 for every type**, including the horizontal I.
The spawn region above row 20 is rendered so the initial positions can be inspected.
There is no automatic one-row fall.

In human-readable, one-based columns, I occupies 4–7, O occupies 5–6, and the
three-wide pieces occupy 4–6. Odd-width pieces round left rather than straddling
cell boundaries. The north-facing shapes have J/L/T flat-side down, S rising right,
Z rising left, O square, and I horizontal. This matches the reference screenshot
and the starting-orientation descriptions in the
[2009 design guideline, sections 3.3–3.4 (third-party hosted copy)](https://studylib.net/doc/27262492/2009-tetris-design-guideline).
Only spawn orientation is implemented, not SRS rotation or wall kicks.

The component renders relative to the board root: -X toward the camera, +Y right,
+Z up, 10 Unreal units per cell. NEXT uses five 30-unit slots centered at (Y=80,
Z=11.5). Its meshes are opaque, unlit and non-colliding; no per-frame tick is needed.
`M_Mino` has **Used with Instanced Static Meshes** enabled so the runtime instances
render their per-type `Color` parameters instead of the fallback material.

## Debugging and validation

The component exposes `FirstBagOrder`, `ActivePieceName`, `NextPieceNames`,
`ActiveSpawnCells` and `DebugRestartCount` for inspection while playing. It logs
the sequence at startup and on R. Enable `bUseFixedSeed` for reproducible sessions.

Run the Unreal automation test group `Totoris.Generation`:

- `SevenBagAndPreview`: 1,000 bags, seven unique types per bag, five-item
  non-consuming lookahead, including bag boundaries.
- `DebugRestartCycle`: resets after consuming beyond the first bag, verifies left
  rotation, NEXT order, all seven active types, and wraparound after seven resets.
- `SpawnOrientationsAndRow22`: exact occupied coordinates for all seven types,
  four unique blocks, centered on-grid columns, highest row 22.

Validation: Development Editor build and all three tests passed (zero test warnings
or failures). In PIE, seven actual R keypresses cycled
`ISZLOJT -> SZLOJTI -> ZLOJTIS -> LOJTISZ -> OJTISZL -> JTISZLO -> TISZLOJ -> ISZLOJT`.
Every step matched the expected active piece, five previews, restart counter and
highest row 22. Click the PIE viewport once if keyboard focus is still in the editor.
