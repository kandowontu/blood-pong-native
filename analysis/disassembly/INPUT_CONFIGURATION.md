# Original input configuration

All addresses below are virtual addresses in the supplied legacy executable.
The original executable was inspected as inert data and was not launched.

## Default keyboard layout

The 232-byte configuration block begins at `0x00435A74`. Its default
initializer is `0x0040EEF0`; the player constructor copies the resolved scan
codes into the fighter object at `0x0040D3C9` (player 1) and `0x0040D526`
(player 2).

| Action | Player 1 | DIK scan code | Player 2 | DIK scan code |
|---|---|---:|---|---:|
| Up | W | `0x11` | Up arrow | `0xC8` |
| Down | S | `0x1F` | Down arrow | `0xD0` |
| Back | A | `0x1E` | Left arrow | `0xCB` |
| Forward | D | `0x20` | Right arrow | `0xCD` |
| Attack 1 | 1 | `0x02` | 6 | `0x07` |
| Attack 2 | 2 | `0x03` | 7 | `0x08` |
| Attack 3 | 3 | `0x04` | 8 | `0x09` |
| Turbo | 5 | `0x06` | 0 | `0x0B` |
| Super | 4 | `0x05` | 9 | `0x0A` |

The apparent `Turbo`/`Super` ordering is not inferred from numeric key order:
it follows the configuration-screen labels and the two distinct fighter-field
copies. Attack scan codes are stored at fighter offsets `+0x133` through
`+0x135`; the remaining two buttons are copied to `+0x130` and `+0x136`.

## Runtime dispatch

- The player constructor stores a pointer at fighter offset `+0xD8`; each
  fighter's input-state recognizer reads the current one-byte event through that
  pointer.
- The per-fighter recognizers occupy the dense region beginning at
  `0x004082A4`. They compare queued scan codes with the five fighter button
  fields and advance character-specific combination states.
- The recognizers do not support the earlier native build's provisional `D`
  and left-Control attack bindings. Those bindings have therefore been removed.
- The native port maps XInput `X/A/B` to the three attacks, `Y` to Super, and
  right shoulder to Turbo while retaining the exact original keyboard defaults.

The exact character-specific combination transitions remain represented in the
byte-complete listing and function ledger while their behavioral translation is
in progress.
