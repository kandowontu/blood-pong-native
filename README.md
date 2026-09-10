# Blood Pong native Windows preservation port

Version 1.0.0 · [Download the current Windows release](https://github.com/kandowontu/blood-pong-native/releases/latest)

Player guide: **[Complete movelist for all 16 characters](MOVELIST.md)**

This is a clean native Windows reconstruction of the 1998/1999 Monkey Byte Development release of **Blood Pong**, programmed and illustrated by Brandon Kuroda. The finished program will be one self-contained Windows GUI executable: no original executable, compatibility layer, installer, or loose runtime assets.

## Required compatibility behavior

- `Alt+Enter` toggles borderless fullscreen in the existing window.
- `Ctrl+Alt+F1` activates the correct unlock sequence while the Ultimate Kombat Kode screen is active.
- The same hotkey opens an in-game cheat menu during a match.
- A main-menu Credits screen preserves the original authorship and special
  thanks, while separately identifying the native preservation-port work.
- No visible text advertises the cheat shortcut.
- Original graphics, audio, characters, rules, timing, menus, secrets, and one/two-player behavior are reconstructed from the supplied executable and reference captures.

## Current evidence

The supplied program is a 32-bit PE built with Borland C++ 1995-era tooling. It imports DirectDraw, DirectInput, and DirectSound and contains a 187,904-byte executable `CODE` section plus 1,214 resources. Game-specific media is embedded rather than supplied as loose files: three standard title/loading bitmaps, 51 additional BMP records, 133 Creative Voice audio records, and extensive custom sprite/image banks.

Static analysis is performed without launching the legacy executable. Reproducible extraction and disassembly tools live under `tools/`; authoritative findings live under `analysis/`.

## Source and asset notice

The Git repository intentionally excludes the supplied legacy executable,
archive, extracted graphics/audio, generated resource script, build trees, and
release artifacts. The checked-in source therefore contains no copy of the
original executable or loose game-data resources. To reproduce the standalone
build, provide your own lawfully obtained copy of the original `Blood Pong.exe`
whose SHA-256 is
`58E211918F671409E9C66F5A35FEE6576C8AEA0A75907E3EAFB930C0595A9680`.

The finished release executable embeds the reconstructed runtime assets and
does not execute, load, or distribute the legacy program code at runtime.

## Native reconstruction status

The native build currently includes the original title, selection, versus,
arena and Credits presentation; the exact 16-fighter resource mapping; all
three arena backdrops; concurrent embedded VOC music, announcer, selection
voices, impacts, and character-projectile effects; keyboard and XInput gamepad
controls; gamepad assignment/dead-zone/vibration options; health and super
state using the original 186/154 ranges; animated fighter damage frames; fighter
projectile banks and all 49 constructor-defined components recovered from the
original type switch; fullscreen; and the context-sensitive secret shortcut.
The exact original keyboard layout (three attacks, Turbo and Super), all 59
recovered character move/secret-realm recipes, the original HUD/name resources, the
59-stage ROUND/FIGHT presentation, two-round match flow, and the 28-stage
FINISH HIM/HER sequence and guarded fatality path are also active. One-player
mode uses the four recovered nine-battle ladders, the original KONTINUE panel
and countdown, and the original GAME OVER cues. The post-finisher nine-state
sequence now waits for the recovered fighter, WINS, optional FLAWLESS VICTORY,
and optional FATALITY cues before advancing automatically; it has no added
post-game prompt or secondary window. Presentation uses a persistent scaled
backbuffer with a single final window copy, eliminating the live black clear
that caused flicker. Default ball play now uses the recovered 16x16 center,
five-pixel velocity, 544x432 bounds, 30-point wall damage, collision gates, and
original return-angle branches without invented serve resets or contact damage.
All 29 versus messages and their recovered gameplay modifiers are active,
including the normal secondary ball and wall-only decoy. The full-Super flag,
five-update gauge drain, sixteen-entry fighter-to-effect map, and all ten
temporary ball-status branches are also reconstructed. The gameplay/class-
method audit and native release harness now cover every fighter before the
preservation build is packaged. Fighter-specific projectile and
outer-wall reaction pairs, plus all specialized projectile callbacks, now use
the recovered constructor and state-machine behavior. The shared projectile
path also preserves its launch holds, flight-bank cadences, repeated mode
adjustments, boomerang persistence, and one-hit persistent effects. One-player
CPU play uses the original four ladder thresholds, the two recovered
approach/retreat accumulator state machines, the upper-tier 50..100-pixel
interception and speed-matching branches, and all sixteen character attack
selectors, including simultaneous pairs and position-dependent projectile
modes.

## Reproducible Windows build

The release configuration uses MSVC's static runtime, so the executable needs
no bundled C/C++ runtime DLLs and no loose game assets. Install CMake, Ninja,
Python 3, and Visual Studio 2022 Build Tools with the C++ workload. Then, from
the repository root, prepare the original resources as inert data:

```powershell
python -m pip install -r requirements.txt
New-Item -ItemType Directory -Force "analysis/originals/Blood Pong"
Copy-Item "C:/path/to/original/Blood Pong.exe" "analysis/originals/Blood Pong/Blood Pong.exe"
python tools/extract_pe_resources.py "analysis/originals/Blood Pong/Blood Pong.exe" analysis/extracted
```

Confirm that the extractor reports the expected executable hash above. After
entering a Visual Studio x64 developer shell:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc --config Release
python tools/verify_embedded_resources.py analysis/extracted/manifest.json "build-msvc/bin/Blood Pong.exe"
python tools/check_presentation_stability.py "build-msvc/bin/Blood Pong.exe"
python tools/check_character_matrix.py "build-msvc/bin/Blood Pong.exe"
python tools/audit_audio_resources.py analysis/extracted/type_2001_custom_2001 analysis/disassembly/AUDIO_RESOURCES.json analysis/disassembly/AUDIO_RESOURCES.md
python tools/audit_combo_state_machines.py "analysis/originals/Blood Pong/Blood Pong.exe" analysis/disassembly/combo_recipes.json
python tools/package_release.py "build-msvc/bin/Blood Pong.exe"
```

The final executable imports only standard Windows system libraries
(`KERNEL32`, `USER32`, `GDI32`, `MSIMG32`, and `WINMM`).

## Attribution

Original-game and native-port attribution is preserved in the in-game Credits
screen and in [CREDITS.md](CREDITS.md). This is an unofficial preservation
project and is not affiliated with or endorsed by the original developers or
publisher.
