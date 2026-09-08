# Blood Pong native Windows preservation port

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
and countdown, and the original GAME OVER cues. Presentation uses a persistent scaled
backbuffer with a single final window copy, eliminating the live black clear
that caused flicker. The gameplay/class-method audit is still being used to
replace provisional movement timing and the most specialized secondary
status-effect behavior before a preservation release is declared 1:1 complete.

## Reproducible Windows build

The release configuration uses MSVC's static runtime, so the executable needs
no bundled C/C++ runtime DLLs and no loose game assets. After entering a Visual
Studio x64 developer shell:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc --config Release
python tools/verify_embedded_resources.py analysis/extracted/manifest.json "build-msvc/bin/Blood Pong.exe"
python tools/check_presentation_stability.py "build-msvc/bin/Blood Pong.exe"
python tools/audit_audio_resources.py analysis/extracted/type_2001_custom_2001 analysis/disassembly/AUDIO_RESOURCES.json analysis/disassembly/AUDIO_RESOURCES.md
python tools/audit_combo_state_machines.py "analysis/originals/Blood Pong/Blood Pong.exe" analysis/disassembly/combo_recipes.json
```

The final executable imports only standard Windows system libraries
(`KERNEL32`, `USER32`, `GDI32`, `MSIMG32`, and `WINMM`).
