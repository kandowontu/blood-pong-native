# Versus-kode dispatcher

The six input digits are combined as a decimal integer at `0x00401458` and
stored at `0x0043670C`. The message object at `0x00413EAC` recognizes the
following 29 values. Leading zeroes shown here are significant to the visible
six-slot entry even though the stored value is an integer.

| Kode | Original message | Recovered effect site |
|---|---|---|
| `022067` | WHAT ELSE SHOULD I BE...ALL APOLOGIES | message only |
| `123926` | THERE IS NO KNOWLEDGE THAT IS NOT POWER | message only |
| `082397` | BE HERE NOW | message only |
| `110110` | TINY BALL | ball ctor: type 2004 ID 503, 8x8 |
| `654321` | SLOW BALL | ball ctor: base speed 4 |
| `123456` | FAST BALL | ball ctor: base speed 8 |
| `421421` | PROJECTILE HALF DAMAGE | component ctor: damage shifted right once |
| `124124` | PROJECTILE DOUBLE DAMAGE | component ctor: damage shifted left once |
| `222222` | BALL HALF DAMAGE | ball ctor: 30 becomes 15 |
| `888888` | BALL DOUBLE DAMAGE | ball ctor: 30 becomes 60 |
| `989121` | BALL DISABLED | ball activation is skipped |
| `100100` | INVISIBLE BALL | ball updates with drawing disabled |
| `202202` | SECONDARY BALL | second ball uses the normal callback |
| `414141` | DECOY BALL | second ball uses wall-only callback `0x00413868` |
| `228882` | CRAZY BALL | velocity is randomized every sixth update |
| `990990` | MAMMOTH BALL | ball ctor: type 2004 ID 502, 71x71 |
| `880880` | GIANT BALL | ball ctor: type 2004 ID 501, 38x38 |
| `604406` | RUN DISABLED | five-pixel Turbo vertical boost is suppressed |
| `510510` | PROJECTILES DISABLED | fighter action callback replacement |
| `123987` | HIDDEN BARS | alternate fighter HUD callbacks |
| `035035` | REVERSE KONTROLS | both horizontal and vertical input pairs swapped |
| `555555` | RANDOM PADDLES | visual fighter is randomized after 400 idle updates |
| `711117` | INVISIBLE PADDLES | fighter drawing flag is cleared |
| `033000` | PLAYER 1 HALF ENERGY | player 1 health shifted right once |
| `000033` | PLAYER 2 HALF ENERGY | player 2 health shifted right once |
| `033033` | BOTH PLAYERS HALF ENERGY | both health values shifted right once |
| `707000` | PLAYER 1 QUARTER ENERGY | player 1 health shifted right twice |
| `000707` | PLAYER 2 QUARTER ENERGY | player 2 health shifted right twice |
| `707707` | BOTH PLAYERS QUARTER ENERGY | both health values shifted right twice |

## Separate kode paths

- `111999` is compared at `0x0040147A` and enters the Ultimate Kombat Kode
  interface rather than the gameplay-message dispatcher. The native shortcut
  writes these exact six digits and activates the reconstructed full-version
  state while the kode screen is open.
- `555555` is cleared at `0x0040145D` if the full-version byte is not set. The
  native reconstruction preserves that gate.
- Four additional values call `0x00401B10` only in the full version and set
  one-time hidden-fighter flags: `011276` for original fighter 6, `100175` for
  7, `011182` for 10, and `041067` for 11. The native full-content state makes
  the complete 16-fighter roster available directly.

## Native coverage

All 29 messages are recognized. The native match applies the recovered ball
sprite/speed/damage/visibility modes, normal secondary ball, wall-only decoy,
crazy velocity, projectile scaling and disable, Turbo/run disable, hidden HUD,
reversed movement, random and invisible paddles, and all six health handicaps.
The message-only easter eggs remain message-only. Specialized temporary ball
statuses driven by character attacks are distinct from versus kodes and remain
tracked in `BALL_PHYSICS.md`.
