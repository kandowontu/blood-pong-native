# Blood Pong complete character movelist

This guide covers every player-accessible combination recovered from the 16
original character input recognizers: 43 character specials plus the shared
secret-realm sequence. Move names below are descriptive labels for the
recovered behavior; they are not presented as canonical names from the 1999
release.

## Input legend

| Action | Player 1 keyboard | Player 2 keyboard | XInput gamepad |
|---|---|---|---|
| Move | `W` `A` `S` `D` | Arrow keys | D-pad or left stick |
| Attack 1 (`A1`) | `1` | `6` | `X` |
| Attack 2 (`A2`) | `2` | `7` | `A` |
| Attack 3 (`A3`) | `3` | `8` | `B` |
| Super | `4` | `9` | `Y` |
| Turbo | `5` | `0` | Right shoulder |

Enter each sequence from left to right. Each new input must arrive before the
original 60-update timeout expires—roughly 0.94 seconds at the native update
rate. The inputs are consecutive presses, not chords.

The damage column is the unmodified constructor value against a maximum health
of 186. Versus kodes can halve, double, or disable projectile damage. A dash
means the move uses a status/behavior effect or animation without a conventional
damage value.

## Shared actions

- Holding Turbo while moving vertically increases movement from 8 to 13 pixels
  per update and drains the 58-point Turbo gauge. It does not boost horizontal
  movement.
- Pressing Super with a full 154-point Super gauge arms that character's ball
  effect. The effect is applied on the fighter's next paddle contact while the
  gauge drains.
- During `FINISH HIM/HER`, the winning human player can enter any of their
  listed character-special sequences. A successful special launches the move
  and enters the `FATALITY` result path; there is no separate fatality recipe.
- Every character recognizes `SUPER → SUPER → TURBO → TURBO` for the secret
  realm. It succeeds only for Player 1 in one-player mode, with all content
  unlocked, during active play on original arena 2. It arms the realm transport
  for the match transition.

## Character Super effects

| Character | Full-Super ball effect |
|---|---|
| Fung Shwei | Chaos ball: replaces both velocity components periodically. |
| Lo Than | High-speed ball using an outgoing-angle base of 8. |
| Jewel | Target-zone ball: approaches horizontally at 6, then gains a random signed vertical speed near the opponent. |
| Raptor | Invisible ball until the temporary status clears. |
| So Frio | Proximity reversal: flips vertically when both target-distance tests pass. |
| Nai Palm | Twin ball: launches at 6×4 and creates a mirrored-y auxiliary ball. |
| One Eye | Randomized wall bounce while preserving horizontal direction. |
| Raider | Accelerating ball near the opponent. |
| Show Lin | Chaos ball: replaces both velocity components periodically. |
| Dawg Cau | Return ball: survives the next paddle return, travels 100 pixels, then reverses horizontally. |
| Omoh | Target-zone ball with a randomized vertical approach near the opponent. |
| Carmack | Proximity reversal. |
| Pain | Chaos ball. |
| Lo Pan | Return ball. |
| Mai Lai | Target-zone ball with a randomized vertical approach. |
| Baka | Twin ball with a mirrored-y auxiliary. |

## Fung Shwei

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Long beam (Component 1) | `A1 → A1 → A2 → A2 → TURBO` | 516-pixel beam; sweeps at 14 pixels/update and remains visible until it exits. | 10 |
| Special stance (Component 3) | `A1 → A1 → A3 → A1 → SUPER` | Special animation; no independently initialized projectile. | — |
| Special stance (Component 2) | `A2 → A3 → A2 → A1 → TURBO` | Special animation; no independently initialized projectile. | — |

## Lo Than

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Accelerated crossing shot (Component 3) | `A1 → A1 → A1 → A3 → TURBO` | Large six-frame projectile; mode 16 increases horizontal displacement after crossing. | 30 |
| Standard shot (Component 1) | `A2 → A1 → A2 → A2 → SUPER` | Large six-frame straight projectile. | 30 |
| Drag crossing shot (Component 2) | `A3 → A1 → A1 → A3 → TURBO` | Large six-frame projectile; mode 15 reduces horizontal displacement after crossing. | 30 |

## Jewel

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Boomerang shot (Component 2) | `A1 → A2 → A2 → A3 → SUPER` | Mode-14 projectile travels beyond the side bound, reverses, and returns to normal flight. | 30 |
| Persistent shot (Component 1) | `A2 → A1 → A2 → A1 → SUPER` | Animated projectile that remains active after its guarded first impact. | 30 |
| Rising persistent shot (Component 3) | `A3 → A2 → A1 → A1 → SUPER` | Persistent shot; mode 12 adds upward acceleration after entering the target region. | 30 |

## Raptor

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Animated field (Component 2) | `A1 → A1 → A1 → A2 → SUPER` | Four-step ping-pong flight animation; no conventional damage value. | — |
| Slow animated field (Component 3) | `A1 → A1 → A3 → A2 → TURBO` | Mode-11 form with the alternate start cue and slower travel. | — |
| Thin shot (Component 1) | `A3 → A3 → A2 → A2 → TURBO` | Fast, narrow four-frame projectile. | 30 |

## So Frio

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Ice status shot (Component 1) | `A1 → A2 → A1 → A1 → SUPER` | Six-frame behavior-replacement projectile. | — |
| Rise-and-drop ice (Component 2) | `A2 → A1 → A3 → A3 → SUPER` | Rises at 9 pixels/update, relocates over the far side, then drops. | — |
| Mirrored ice double (Component 3) | `A3 → A3 → A1 → A1 → SUPER` | Copies the paddle on the mirrored side; hidden for its first 20 of 100 updates. | — |

## Nai Palm

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Downward steep extra ball (Component 3) | `A1 → A1 → A3 → A3 → TURBO` | Independent 16×16 ball using the mode-8 diagonal. | 30 |
| Upward shallow extra ball (Component 2) | `A2 → A2 → A2 → A2 → TURBO` | Independent extra ball using the mode-7 diagonal. | 30 |
| Upward steep extra ball (Component 1) | `A3 → A1 → A3 → A1 → SUPER` | Independent extra ball using the mode-6 diagonal. | 30 |

## One Eye

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Special stance (Component 3) | `A1 → A1 → A2 → A3 → TURBO` | Special animation; no independently initialized projectile. | — |
| Energy shot (Component 1) | `A3 → A1 → A2 → A1 → TURBO` | Four-frame conventional projectile. | 20 |
| Traveling paddle double (Component 2) | `A3 → A3 → A2 → A2 → TURBO` | A copy of the paddle travels horizontally, mirrors on contact, and continues offscreen. | — |

## Raider

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Variant-6 shot (Component 2) | `A1 → A1 → A3 → A1 → TURBO` | Seven-frame projectile using constructor variant 6. | 30 |
| Variant-4 shot (Component 1) | `A2 → A2 → A2 → A3 → TURBO` | Seven-frame projectile with the four-step ping-pong flight path. | 30 |
| Special stance (Component 3) | `A3 → A3 → A2 → A1 → SUPER` | Special animation; no independently initialized projectile. | — |

## Show Lin

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Light shot (Component 3) | `A1 → A1 → A1 → A3 → SUPER` | Six-frame straight projectile. | 10 |
| Heavy shot (Component 1) | `A1 → A2 → A2 → A1 → SUPER` | Six-frame straight projectile. | 30 |
| Medium shot (Component 2) | `A3 → A3 → A2 → A3 → SUPER` | Six-frame straight projectile. | 15 |

## Dawg Cau

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Far-side strike B (Component 2) | `A2 → A2 → A3 → A1 → SUPER` | Spawns low in the opponent's half using mode 26, rises at 4, and drifts/reflects within that half. | 15 |
| Giant edge effect (Component 3) | `A2 → A2 → A3 → A3 → SUPER` | 60-stage bottom-anchored far-side effect with a changing collision rectangle. | 20 |
| Far-side strike A (Component 1) | `A3 → A2 → A1 → A1 → TURBO` | Mode-25 counterpart of the rising far-side strike. | 15 |

## Omoh

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Accelerating lob (Component 1) | `A1 → A2 → A1 → A2 → SUPER` | Gravity-staged 20×20 lob using the standard horizontal speed. | 30 |
| Fast accelerating lob (Component 2) | `A3 → A2 → A3 → A2 → SUPER` | Mode-22 lob using the faster horizontal speed. | 30 |

## Carmack

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Level shot (Component 1) | `A2 → A2 → A1 → A3 → TURBO` | Three-frame conventional projectile. | 30 |
| Rising shot (Component 2) | `A3 → A2 → A3 → A1 → SUPER` | Mode-17 form starts with upward vertical velocity. | 30 |

## Pain

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Animated apparition (Component 1) | `A1 → A2 → A3 → A2 → TURBO` | Five-frame variant-4 projectile with a ping-pong flight cycle. | 20 |
| Full-height apparition (Component 2) | `A2 → A2 → A1 → A1 → TURBO` | Far-side 140×432 effect that scans the opponent vertically in 30-pixel steps. | — |

## Lo Pan

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Guided persistent shot (Component 1) | `A1 → A2 → A2 → A3 → SUPER` | Mode-23 shot follows one owner vertical input by 4 pixels and persists after its first guarded hit. | 30 |
| Straight persistent shot (Component 2) | `A3 → A3 → A2 → A2 → TURBO` | Standard mode of the same five-frame persistent projectile. | 30 |

## Mai Lai

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Slow random-bounce shot (Component 2) | `A1 → A1 → A1 → A1 → SUPER` | Mode-11 three-frame projectile with randomized 5–20 pixel vertical displacement. | 30 |
| Special stance (Component 3) | `A2 → A1 → A1 → A2 → TURBO` | Special animation; no independently initialized projectile. | — |
| Random-bounce shot (Component 1) | `A3 → A2 → A2 → A3 → SUPER` | Three-frame projectile with reflected randomized vertical displacement. | 30 |

## Baka

| Move | Exact sequence | Recovered behavior | Damage |
|---|---|---|---:|
| Projectile B (Component 2) | `A1 → A2 → A3 → A3 → SUPER` | Four-frame type-23 conventional projectile. | 20 |
| Projectile A (Component 1) | `A3 → A2 → A2 → A2 → TURBO` | Four-frame type-22 conventional projectile. | 20 |

## Recovery notes

The sequences were obtained by deterministic emulation of every original
input-state recognizer, not guessed from animation or key patterns. The source
evidence and machine-readable data are in
[`analysis/disassembly/COMBO_RECIPES.md`](analysis/disassembly/COMBO_RECIPES.md),
[`combo_recipes.json`](analysis/disassembly/combo_recipes.json), and
[`CHARACTER_TABLES.md`](analysis/disassembly/CHARACTER_TABLES.md).

Some constructors contain extra projectile variants used by CPU attack
selection but have no accepting human-input recipe. Those are implementation
behaviors, not omitted player moves.
