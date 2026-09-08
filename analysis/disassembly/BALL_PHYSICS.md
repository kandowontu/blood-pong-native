# Ball construction and collision rules

All addresses refer to the supplied `Blood Pong.exe` at image base
`0x00400000`. The executable was inspected as inert data and was never run.

## Construction and update

- `0x0041294C` constructs the ball from type 2004, resource 500. Its sprite is
  16x16 and its initial top-left position is `(264, 208)`, centering it in the
  544x432 playfield.
- The default base speed is five pixels per update and default wall damage is
  `0x1e` (30). Separate `GetTickCount` bit-zero tests select the initial sign
  of each velocity component.
- The constructor changes the base speed to eight for kode `123456` and four
  for kode `654321`. It changes wall damage to 60 for kode `888888` and 15 for
  kode `222222`.
- `0x00412D48` adds the signed x/y velocity to the integer sprite rectangle and
  then calls the configured collision callback.
- Two one-bit collision gates prevent a ball that remains inside a paddle from
  retriggering that same paddle. Contact with player 1 disables player 1's
  gate and enables player 2's; player 2 does the reverse. A playfield-wall
  collision re-enables both gates.

## Default collision callback (`0x00412DC4`)

The default callback uses the fighter object's fixed 12x54 collision rectangle,
not the potentially wider standing artwork.

- Crossing x=0 damages player 1 and crossing x=544 damages player 2. The ball
  clamps to the edge, reverses x velocity, and remains live; there is no serve
  reset after a point.
- Crossing y=0 or y=432 clamps the ball and reverses y velocity without damage.
- A normal player-1 return changes x velocity to `base - 1` and y velocity to
  `-(base + 1)` above the paddle's central zone, uses `base - 1` and
  `base + 1` below it, or uses `base` on both axes through the center while
  preserving y direction.
- Player 2 mirrors the x signs and uses the same vertical-zone rules.
- If a ball overlaps a paddle while already moving away from it, the callback
  uses `base + 1` horizontally and `base - 1` vertically, reversing the
  vertical direction. The collision gates make this a single correction rather
  than a repeated acceleration.
- Paddle contact does not damage either fighter and does not add Super energy.
  Resource 501 is the shared paddle-contact sound; resource 500 is the vertical
  wall sound. Outer-wall damage chooses a cue from the damaged fighter's sound
  table.

## Alternate and modifier paths

- The alternate callback at `0x00413868` reflects at all four walls without
  fighter damage or paddle tests.
- Kode `414141` selects the alternate callback for the secondary ball.
  Kodes `414141` and `202202` activate that secondary ball when the ROUND/FIGHT
  sequence releases play; its initial x velocity is the primary ball's inverse.
- Kode `228228` periodically replaces both velocity components from
  `GetTickCount` low bits every sixth update.
- The default callback also has ten temporary ball-status branches connected
  to fighter fields at offsets `+0x754` and `+0x77c`. Those specialized
  status-effect semantics and the complete secondary/decoy-ball presentation
  remain open audit items.

## Native reconstruction

The native match loop now uses the recovered center, speed, damage, playfield
bounds, fixed fighter collision rectangles, two collision gates, and all base
outgoing-angle rules. It also removes the earlier reconstructed serve reset,
contact damage, Super gain, and artificial per-return acceleration. The four
speed/damage kode overrides, second-ball modes, temporary status branches, and
fighter-specific outer-wall damage cues are intentionally documented as
remaining work rather than being presented as exact.
