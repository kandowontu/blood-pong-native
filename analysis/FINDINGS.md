# Static-analysis findings

All virtual addresses refer to the supplied `Blood Pong.exe` with image base
`0x00400000`. The legacy executable was parsed as inert data and was not run.

## Display and asset model

- The original requests a `640x480x8` DirectDraw display.
- Full-screen art is 544×432 and is centered at `(48, 24)` inside that display.
- All 54 BMP resources share the same 256-color palette.
- The PE contains 1,214 leaf resources, including 133 Creative Voice (`.VOC`)
  sounds and 1,019 valid proprietary sprite streams.
- The native build embeds all 1,207 numeric game-data resources from custom
  types 1001–2023. It never embeds or executes the original program code.

## Function-level audit coverage

- `LISTING.asm` accounts for every byte in the 187,904-byte `CODE` section.
- `audit.json` and `FUNCTION_INDEX.md` ledger all 1,250 evidence-ranked entry
  candidates: 555 direct-call/entry/export candidates, 651 relocation-backed
  candidates, and 44 otherwise-unreferenced compiler-style prologues.
- Every candidate range is independently hashed and scanned for instruction
  rows, undecodable byte rows, direct calls, strings, and imported-API
  references. The automatic inventory currently classifies 805 ranges as
  game/engine, 422 as compiler/runtime, 8 as audio, 6 as graphics/display, 5 as
  input, and 4 as registration/file related.
- The import surface contains no networking, process creation, injection,
  service, or persistence APIs. Original file activity is limited to ordinary
  registration/runtime file functions; the native port does not carry over the
  shareware registration-file dependency.
- Because Borland symbols were stripped, the ledger distinguishes candidate
  boundary confidence from manual semantic naming instead of presenting
  inferred boundaries as original debug symbols.

## Proprietary sprite format

The renderer at `0x00402F68` interprets a DWORD command stream. The high byte
is an opcode and the low 24 bits are a count:

| Opcode | Meaning |
|---:|---|
| 0 | end image |
| 1 | begin next scanline |
| 2 | copy `count` literal palette indices; source payload is DWORD-aligned |
| 3 | skip `count` transparent destination pixels |

`tools/decode_sprite_streams.py` independently implements this algorithm and
round-trips every custom image bank: 1,019 resources decode cleanly, while only
the eight non-sprite icon/version/dialog records are rejected.

## Versus-kode logic

- `0x00401094` owns the six-slot versus-kode UI.
- `0x00401458` combines the six digits as a decimal integer and stores it at
  `0x0043670C`.
- `0x00413EAC` dispatches 29 versus effects/messages. `VERSUS_KODES.md`
  records every decimal value, displayed string, and recovered effect site;
  the separate `111-999` Ultimate Kombat Kode path makes 30 recognized paths.
- `555-555` maps to `RANDOM PADDLES`; it is not the all-content unlock.
- `111-999` has a separate transition path at `0x0040147A`.
- Match setup checks additional codes including `414-141`, `124-124`, and
  `421-421` outside the message dispatcher.

Static cross-reference auditing confirms that no versus-kode branch writes the
full-version byte. The native shortcut therefore activates the reconstructed
equivalent of the full-version flag directly while on the kode screen; it does
not spoof `555-555` or mislabel a versus modifier as the unlock sequence. It
also fills the visible slots with the exact `111-999` value before activation.

The native two-player match now applies every recovered dispatcher effect:
ball sprite/speed/damage/visibility, secondary and decoy balls, crazy velocity,
projectile damage/disable, run disable, hidden HUD, reversed movement, random
or invisible paddles, and all health handicaps. The `555-555` full-version gate
is retained.

## Character animation and projectile banks

- Each of the 16 normal character resource types contains a uniform gameplay
  animation block at IDs `3000` through `3023`.
- The original constructor jump table at `0x0040D6F6` proves the non-numeric
  fighter/type order: Fung Shwei/2017, Lo Than/2006, Jewel/2005, Raptor/2014,
  So Frio/2016, Nai Palm/2010, One Eye/2011, Raider/2015, Show Lin/2018, Dawg
  Cau/2000, Omoh/2012, Carmack/2002, Pain/2013, Lo Pan/2008, Mai Lai/2020, and
  Baka/2009. The native loader uses this exact mapping.
- The projectile initializer at `0x0041AC54` dispatches through a 25-way type
  switch at `0x0041ACB5`. `PROJECTILE_TYPES.md` records every handler, callback,
  frame pointer, embedded resource bank and declared hit-box size. Most visuals
  come from type 2022; type 9 intentionally reuses the type-2004 ball sprite,
  types 8 and 11 copy the owner's current paddle sprite, and type 0 is not given
  a visual pointer by its handler.
- `CHARACTER_TABLES.md` records all 49 literal projectile initializers from the
  16 fighter constructors, including type, variant, mode and damage. The native
  component table follows all of these constructor entries rather than assuming
  that numerically ordered art banks correspond to roster order.
- `COMBO_RECIPES.md` records all 59 accepting input paths recovered by
  deterministic emulation of the 16 original recognizers: 43 component moves
  and 16 paths to the same guarded secret-realm routine. Each recognizer uses
  the original 60-update input window. `SUPER, SUPER, TURBO, TURBO` calls
  `0x00408248`, which succeeds only in one-player mode, with the full-version
  byte set, on original arena number 2. It records the transport flag and later
  forces original fighter number 7; it is not itself a fatality activator.
- Fighter health initializes to `0xBA` (186) at `0x0040D310`; the super field is
  capped at `0x9A` (154), including at `0x0040CC40`. The native meters use these
  original ranges. The adjacent `+0x100` field initializes to `0x3A` (58) and is
  represented by the native Turbo gauge.
- Damage reactions, health/super state, character projectiles, keyboard input,
  XInput assignment/dead-zone handling, and vibration now run in the native
  match loop. Projectile damage comes from the original constructor arguments.
  The special callback pass has translated the 516-pixel Fung Shwei beam, Nai
  Palm's four extra-ball trajectories, So Frio's rising/drop and mirrored ice
  forms, One Eye's moving double, Show Lin/Dawg Cau's giant effect, Omoh's
  accelerating lob, Pain's full-height apparition, and Dawg Cau's rising column.
  Some secondary status semantics remain subject to semantic comparison as the
  class-method audit progresses.

## Input and match presentation

- `INPUT_CONFIGURATION.md` documents the original DirectInput defaults and the
  fighter-field copies that distinguish Attack 1/2/3, Turbo and Super. The
  native keyboard defaults now match the original (`W/S`, `1/2/3`, `5`, `4`
  for player 1 and arrows, `6/7/8`, `0`, `9` for player 2).
- `0x0040B431` consumes the four recovered direction fields at an exact
  eight-pixel step. Turbo adds five pixels only to vertical movement, drains two
  of 58 units per update, and otherwise regenerates one unit per update. Fighter
  construction establishes the exact half-court x bounds `0..200` and
  `344..544`, the shared y bounds `0..432`, and initial x positions 50/482. The
  native match now preserves these four-direction constraints for keyboard and
  both XInput axes.
- `BALL_PHYSICS.md` records the constructor and default collision callback at
  `0x0041294C` and `0x00412DC4`. The native match now starts the 16x16 ball at
  `(264,208)` with independently signed five-pixel velocity, uses the original
  544x432 bounds and 30-point outer-wall damage, keeps the ball live after wall
  damage, and applies the exact 12x54 paddle gates and outgoing-angle branches.
  It no longer invents contact damage, Super gain, serve resets, or acceleration
  on every return. The ten temporary ball statuses and second/decoy-ball kode
  modes remain explicit audit items.
- `0x00413CF8` loads the type-2023 ROUND, digits 1–3, and FIGHT banks.
  `0x004141EC` advances them on a five-update cadence, swaps banks at stages 19
  and 37, holds the middle FIGHT frame during stages 43–49, and releases play at
  stage 59. The native match start follows this same stage sequence and uses the
  original embedded frames.
- The health, Turbo, Super, round-win, and fighter-name HUD art now comes from
  original type-2004 IDs 128 and 400–403 plus type-2023 IDs 600–615.
- The shared projectile activation routine at `0x0041B8F4` is a callback
  dispatcher, not a single straight-line flight rule. The native state now
  retains the three-update launch banks, the `0x0041BAD8` repeated mode table,
  the variant-four eight-update ping-pong path, the type-3/type-18 four-update
  path, and type 15's six-update animation with random 5..20-pixel vertical
  displacement. Type 3 and type 21 survive their guarded first impact as in
  the original.
- On a knockout, the original increments the winning fighter's `+0x774` round
  count, starts a new round after just over 200 updates, and enters the finish
  path at two wins. `0x004143C4` selects FINISH HER for fighter numbers 3, 10,
  and 16; `0x00414484` drives the 28-stage/140-update prompt. The native match
  state follows that structure and uses the exact type-2023 frames.
- At the end of the FINISH prompt, `0x00414484` has a 50% fallback that activates
  winner component 1 or 2 only when the winner is CPU-controlled. An ordinary
  component recipe entered by the winner during the prompt also enters the
  fatality result path. The separate type-2023
  ID 250 graphic spells `FATALITY`; it is not evidence that the guarded
  `0x00408248` realm routine is a fatality.
- `0x0041459C` is the nine-state post-finisher result object. It starts the
  winner's character cue, waits 21 updates, plays WINS (VOC 250), optionally
  waits 21 and plays FLAWLESS VICTORY (VOC 1007) when the winner retains all
  186 health points, then waits 21 and conditionally plays FATALITY (VOC 1008).
  State 9 holds 51 updates and exits automatically; there is no Enter prompt.
- `0x004027A8` writes four alternative one-player ladders of nine opponents.
  The fifth slot is always original fighter number 10 and the ninth is always
  number 6. The native tournament view uses type-2007 ID 302 at the exact
  recovered portrait coordinates; its continue path uses panel ID 300 and
  digit IDs 400-409 with five initial credits and the original 40-update
  countdown cadence. `0x00416B8C` supplies the GAME OVER path and random
  5002/5003 cue.
- The native presenter never clears the live window. It scales the completed
  640×480 frame and its letterbox bars into a persistent client-sized buffer,
  then performs one final `BitBlt`; `WM_ERASEBKGND` remains suppressed. The game
  timer uses the 15.6 ms Windows cadence instead of allowing a requested 16 ms
  interval to quantize to two ticks on affected systems.

## Audio

- `AUDIO_RESOURCES.md` catalogs all 133 custom type-2001 Creative VOC assets
  and the recovered call-site bindings. All decode as unsigned 8-bit mono PCM;
  every resource referenced by the native sound table is present.
- The original loader at `0x0040AA64` establishes the sixteen character voices
  in fighter-constructor order. `0x0041A570` loads the projectile cue bank used
  by the 25-way component dispatcher. The native tables preserve both orders.
- The native renderer feeds a four-buffer, 22,050 Hz waveOut stream and mixes
  looping music with eight concurrent effect/voice channels. This avoids the
  single-channel `PlaySound` limitation while retaining the original VOC sample
  bytes and rates. ROUND, round-number, FIGHT, FINISH HIM/HER and FATALITY cues
  are attached to their recovered animation/state transitions.

## Registration/full-version gate

- The full-version flag is byte `0x00435B5C`.
- Startup sets it after validating `MBREG.DAT` in the routine at `0x0040ED48`.
- The registration-dialog path writes it at `0x00402241` after validation.
- The title menu and character availability branch on this flag.

## Original credits

The original credit roll begins in DATA around `0x0043116A`. It identifies A
Kuroda Production; Brandon Kuroda for programming and graphics; the group SOUR!
for music; and the special-thanks list transcribed in `CREDITS.md`.
