# Projectile type switch

Recovered from the 25-way switch at `0x0041ACB5`. Resource banks are cross-referenced against the exact loader loop at `0x0041A7D4`. `owner paddle` means the handler deliberately copies the firing fighter's current sprite pointer.

| Type | Handler | Callback | Visual source | Resource bank | Size |
|---:|---:|---:|---|---|---:|
| 0 | `0x0041B077` | `0x0041C7E8` | none | — | — |
| 1 | `0x0041B5AB` | `0x0041C808` | embedded resource | type 2022, IDs 300–301 | 516×16 |
| 2 | `0x0041AEFB` | `0x0041B8F4` | embedded resource | type 2022, IDs 700–705 | 86×29 |
| 3 | `0x0041ADFB` | `0x0041B8F4` | embedded resource | type 2022, IDs 1300–1305 | 34×29 |
| 4 | `0x0041B492` | `0x0041B8F4` | embedded resource | type 2022, IDs 160–166 | 32×56 |
| 5 | `0x0041B51F` | `0x0041B8F4` | embedded resource | type 2022, IDs 180–183 | 80×9 |
| 6 | `0x0041B7C4` | `0x0041B8F4` | embedded resource | type 2022, IDs 128–133 | 72×40 |
| 7 | `0x0041B851` | `0x0041C5F0` | embedded resource | type 2022, IDs 150–151 | 24×91 |
| 8 | `0x0041B77D` | `0x0041C4CC` | owner paddle | — | 12×54 |
| 9 | `0x0041B0A0` | `0x0041D328` | embedded resource | type 2004, IDs 500 | 16×16 |
| 10 | `0x0041B150` | `0x0041B8F4` | embedded resource | type 2022, IDs 1500–1503 | 72×35 |
| 11 | `0x0041B0F2` | `0x0041CFCC` | owner paddle | — | 12×54 |
| 12 | `0x0041B2CA` | `0x0041B8F4` | embedded resource | type 2022, IDs 1200–1206 | 80×32 |
| 13 | `0x0041B67F` | `0x0041B8F4` | embedded resource | type 2022, IDs 200–205 | 48×42 |
| 14 | `0x0041B637` | `0x0041CA58` | embedded resource | type 2022, IDs 1000–1014 | 200×124 |
| 15 | `0x0041B70F` | `0x0041B8F4` | embedded resource | type 2022, IDs 600–602 | 28×28 |
| 16 | `0x0041B1DE` | `0x0041B8F4` | embedded resource | type 2022, IDs 210–215 | 48×42 |
| 17 | `0x0041B26E` | `0x0041D1DC` | embedded resource | type 2022, IDs 170 | 20×20 |
| 18 | `0x0041AD8D` | `0x0041B8F4` | embedded resource | type 2022, IDs 400–402 | 40×40 |
| 19 | `0x0041B394` | `0x0041B8F4` | embedded resource | type 2022, IDs 1550–1554 | 40×35 |
| 20 | `0x0041B404` | `0x0041D694` | embedded resource | type 2022, IDs 1600 | 140×432 |
| 21 | `0x0041AE69` | `0x0041B8F4` | embedded resource | type 2022, IDs 350–354 | 88×23 |
| 22 | `0x0041AFFF` | `0x0041B8F4` | embedded resource | type 2022, IDs 260–263 | 64×26 |
| 23 | `0x0041AF87` | `0x0041B8F4` | embedded resource | type 2022, IDs 250–253 | 64×26 |
| 24 | `0x0041AD19` | `0x0041D49C` | embedded resource | type 2022, IDs 500–502 | 14×62 |
