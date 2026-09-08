# Audio resource audit

All 133 resources of custom PE type 2001 are Creative VOC, unsigned 8-bit mono PCM. The native mixer converts them to a concurrent 22,050 Hz/16-bit output stream at playback time; source bytes remain embedded unchanged.

## Recovered call-site bindings

- `0x00413BD0` loads announcer IDs 1000-1008 and 250.
- `0x004141EC` plays ROUND at stage 1, the round-number voice at stage 19, and FIGHT at stage 37.
- `0x004143C4` selects FINISH HIM (1005) or FINISH HER (1006).
- `0x0041459C` serializes the winner voice, WINS (250), optional FLAWLESS
  VICTORY (1007), and optional FATALITY (1008), waiting for each cue before
  advancing its nine-state result object.
- `0x0040AA64` loads the sixteen character voices and UI cues 3001/3002.
- `0x0041A570` loads the projectile sound bank used by the 25-way dispatcher.
- Fighter constructor fields `+0xA0` and `+0xA4` point into two-sound reaction
  pairs in the sixteen-character bank. Projectile-damage callbacks use `+0xA0`;
  outer ball-wall hits at `0x004130BC` and `0x004131D4` use `+0xA4` and choose
  between the pair with `GetTickCount() & 1`.

### Character-selection voices

| Fighter | Resource ID | Duration |
|---|---:|---:|
| Fung Shwei | 2000 | 0.533 s |
| Lo Than | 2004 | 0.933 s |
| Jewel | 2001 | 0.458 s |
| Raptor | 2005 | 0.514 s |
| So Frio | 2002 | 0.847 s |
| Nai Palm | 2007 | 0.962 s |
| One Eye | 2003 | 0.656 s |
| Raider | 2006 | 0.515 s |
| Show Lin | 2010 | 0.414 s |
| Dawg Cau | 2014 | 0.458 s |
| Omoh | 2011 | 0.241 s |
| Carmack | 2015 | 0.226 s |
| Pain | 2012 | 0.178 s |
| Lo Pan | 2017 | 0.164 s |
| Mai Lai | 2013 | 0.205 s |
| Baka | 2016 | 0.201 s |

### Fighter reaction-pair indices

The indices below address the character-selection bank in the exact order of
the preceding table. Each entry names the first of two adjacent sounds.

| Fighter | Projectile pair | Ball-wall pair |
|---|---:|---:|
| Fung Shwei | 14 | 6 |
| Lo Than | 12 | 4 |
| Jewel | 10 | 2 |
| Raptor | 14 | 6 |
| So Frio | 14 | 6 |
| Nai Palm | 8 | 0 |
| One Eye | 8 | 0 |
| Raider | 8 | 0 |
| Show Lin | 8 | 0 |
| Dawg Cau | 10 | 2 |
| Omoh | 8 | 0 |
| Carmack | 14 | 6 |
| Pain | 14 | 6 |
| Lo Pan | 8 | 0 |
| Mai Lai | 14 | 6 |
| Baka | 10 | 2 |

### Projectile start cues

| Original type | Resource ID |
|---:|---:|
| 1 | 3000 |
| 2 | 3003 |
| 3 | 3005 |
| 4 | 3008 |
| 5 | 3007 |
| 6 | 3010 |
| 7 | 3010 |
| 8 | 3012 |
| 9 | 3032 |
| 10 | 3013 |
| 11 | 2050 |
| 12 | 3014 |
| 13 | 3019 |
| 14 | 3020 |
| 15 | 3013 |
| 16 | 3012 |
| 17 | 3021 |
| 18 | 3031 |
| 19 | 3030 |
| 20 | 3029 |
| 21 | 3023 |
| 22 | 3025 |
| 23 | 3025 |
| 24 | 3020 |

Type 4 uses 3028 instead of 3008 in mode 11. Type 7 plays 3011 when its rising phase changes into the falling phase.

Referenced resource validation: PASS
