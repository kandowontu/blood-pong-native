# Blood Pong Native v1.0.0

The first preservation release is a native 64-bit Windows reconstruction of
the 1998/1999 Monkey Byte Development game. The downloadable EXE is completely
self-contained: it needs no installer, compatibility layer, legacy executable,
loose assets, or separately installed C/C++ runtime.

## Highlights

- Original 640×480 presentation, all 16 fighters, three arenas, graphics,
  voices, music, projectiles, combos, fatalities, versus kodes, tournament
  ladders, secret realm, and post-match sequencing.
- One-player CPU behavior with all four recovered difficulty callbacks and all
  16 fighter-specific attack selectors.
- Two-player keyboard and XInput gamepad play, including controller assignment,
  stick dead-zone, vibration, and audio settings.
- Borderless fullscreen toggle with `Alt+Enter`.
- Context-sensitive hidden unlock/cheat functionality without an on-screen
  shortcut advertisement.
- Persistent single-copy presentation path to prevent blank-frame flicker.
- Original credits retained in-game and separated from preservation-port
  attribution.

## Verification

- All 1,207 numeric game-data resources are embedded and byte-identical to the
  statically extracted inputs.
- One recovered component recipe was exercised successfully for every fighter
  in a 16/16 native runtime matrix.
- Windowed and fullscreen presentation passed high-frequency blank-frame
  sampling.
- The executable imports only standard Windows system libraries: `KERNEL32`,
  `USER32`, `GDI32`, `MSIMG32`, and `WINMM`.

The supplied legacy executable was analyzed as inert data and was never run.
This is an unofficial preservation project and is not affiliated with or
endorsed by the original developers or publisher.
