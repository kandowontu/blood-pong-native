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
- `0x00413EAC` dispatches the 30 known versus effects/messages.
- `555-555` maps to `RANDOM PADDLES`; it is not the all-content unlock.
- `111-999` has a separate transition path at `0x0040147A`.
- Match setup checks additional codes including `414-141`, `124-124`, and
  `421-421` outside the message dispatcher.

The exact all-content/registration activation path remains under audit. The
native shortcut must not claim that `555-555` is the unlock code.

Static cross-reference auditing confirms that no versus-kode branch writes the
full-version byte. The native shortcut therefore activates the reconstructed
equivalent of the full-version flag directly while on the kode screen; it does
not spoof `555-555` or mislabel a versus modifier as the unlock sequence.

## Character animation and projectile banks

- Each of the 16 normal character resource types contains a uniform gameplay
  animation block at IDs `3000` through `3023`.
- The original constructor jump table at `0x0040D6F6` proves the non-numeric
  fighter/type order: Fung Shwei/2017, Lo Than/2006, Jewel/2005, Raptor/2014,
  So Frio/2016, Nai Palm/2010, One Eye/2011, Raider/2015, Show Lin/2018, Dawg
  Cau/2000, Omoh/2012, Carmack/2002, Pain/2013, Lo Pan/2008, Mai Lai/2020, and
  Baka/2009. The native loader uses this exact mapping.
- The shared projectile type 2022 is organized into 16 ordered animation
  groups. The native combat slice loads each complete group rather than using
  an invented replacement graphic.
- Damage reactions, health/super state, character projectiles, keyboard input,
  XInput assignment/dead-zone handling, and vibration now run in the native
  match loop. Exact original timing/damage constants remain subject to semantic
  comparison as the class-method audit progresses.

## Registration/full-version gate

- The full-version flag is byte `0x00435B5C`.
- Startup sets it after validating `MBREG.DAT` in the routine at `0x0040ED48`.
- The registration-dialog path writes it at `0x00402241` after validation.
- The title menu and character availability branch on this flag.

## Original credits

The original credit roll begins in DATA around `0x0043116A`. It identifies A
Kuroda Production; Brandon Kuroda for programming and graphics; the group SOUR!
for music; and the special-thanks list transcribed in `CREDITS.md`.
