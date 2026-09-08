# One-player CPU behavior

The common fighter initializer at `0x0040D0C8` selects one of four movement
callbacks from ladder difficulty value `0x004366AC`:

| Ladder value | Callback | Special-attack cadence | Full-Super cadence |
|---:|---:|---|---|
| 0..1 | `0x004108A4` | accumulate `rand() & 255` past `0x61A8` | accumulate `rand() & 255` past `0x7530` |
| 2..3 | `0x00410BA8` | accumulate `rand() & 15` past 1000, then wait for the ball beyond the near bound | accumulate `rand() & 15` past 1000 |
| 4..5 | `0x00410E84` | direct character-callback opportunities | immediate |
| 6..8 | `0x00411290` | direct character-callback opportunities with the most aggressive interception path | immediate |

All four move in both axes. The first two use `abs(ball.vy)` inside the
20-pixel vertical band, otherwise the fighter's eight-pixel movement field.
The upper two use the five-pixel Turbo addition on interception branches. If
the active ball has no horizontal velocity, the low tiers follow the opposing
fighter vertically. The secret-realm constructor forces the fourth callback
for fighter IDs 6, 7, 10, and 11.

## Character attack selectors

The sixteen `+0x6C` callbacks occupy `0x0041182C..0x004128C9`. An active flag
is at projectile-object offset `+0x3C`; the four possible object bases are
fighter offsets `+0x144`, `+0x1FC`, `+0x2B4`, and `+0x36C`.

| Fighter | Recovered choice behavior |
|---|---|
| Fung Shwei | vertical-distance/approach guard; 2-in-8 attempt, component 4 at the far wall, otherwise 1 |
| Lo Than | vertical-distance/approach guard; equally chooses components 1, 2, 3, or no attack |
| Jewel | component 3 above, 4 below, otherwise random 1 or 2 |
| Raptor | approach-sensitive 2/3 defense; otherwise up to four choices from `rand() & 7` |
| So Frio | 1-in-4 component 1, 1-in-4 component 3, otherwise component 2 with a randomized drop variant |
| Nai Palm | equally chooses simultaneous pairs 1+2, 3+4, 1+3, or 2+4 |
| One Eye | vertical-distance/approach guard; random component 1 or 2 |
| Raider | approach-sensitive component 2; otherwise sparse 1/2 attempts |
| Show Lin | four-way component-1 modes 0/19/20 or component 4, with a far-offset component-4 fallback |
| Dawg Cau | random simultaneous 1+2 or component 3 |
| Omoh | 1-in-4 component 2, 1-in-4 component 3, otherwise component 1 |
| Carmack | component 2 above, 3 below, 4 at the far wall, otherwise 1 |
| Pain | lower-side distance guard; random component 1 or 2 |
| Lo Pan | random component 1 or 2; component 1 uses mode 12 above, 13 below, or 0 when aligned |
| Mai Lai | component 3 at the far wall; otherwise component 1, falling back to 2 while 1 is active |
| Baka | components 1+2 together, with modes 12/13 selected from vertical relation or a random aligned branch |

The native port preserves the four threshold tiers, paired launches, component
mode overrides, vertical/far-wall guards, and per-fighter random dispatch.
Transient original animation-state guards are represented by the native damage
and freeze reaction states; this avoids launching new attacks through a target
reaction without inventing inaccessible legacy object state.
