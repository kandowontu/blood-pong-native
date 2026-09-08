# Fighter constructor table

Recovered from the original 17-way constructor jump table at `0x0040D6F6`. Projectile type/damage arguments are literal parameters passed to `0x0041AC54`; blank entries are dynamically configured or use a different initializer.

The three callback columns are the exact code pointers assigned to fighter fields
`+0x6C`, `+0x78`, and `+0x84`. Cross-references from the four one-player
movement callbacks and the human-input dispatcher establish their roles as the
CPU attack selector, direct attack-key handler, and combo recognizer.

| # | Fighter | Resource type | Constructor | Paddle pointer | CPU attack +6C | Attack keys +78 | Combo +84 | Projectile type:damage |
|---:|---|---:|---:|---:|---:|---:|---:|---|
| 1 | Fung Shwei | 2017 | `0x0040D73A` | `0x0043565C` | `0x0041182C` | `0x00406524` | `0x004082A4` | 1:10, 0:0, 0:0, 0:0 |
| 2 | Lo Than | 2006 | `0x0040D850` | `0x00435674` | `0x004118BC` | `0x00406608` | `0x00408538` | 2:30, 2:30, 2:30 |
| 3 | Jewel | 2005 | `0x0040D94F` | `0x0043568C` | `0x004119E8` | `0x00406764` | `0x004087DC` | 3:30, 3:30, 3:30, 3:30 |
| 4 | Raptor | 2014 | `0x0040DA76` | `0x004356A4` | `0x00411B04` | `0x00406954` | `0x00408A7C` | 5:30, 4:0, 4:0, 0:0 |
| 5 | So Frio | 2016 | `0x0040DB9D` | `0x004356BC` | `0x00411D34` | `0x00406AF4` | `0x00408D10` | 6:0, 7:0, 8:0 |
| 6 | Nai Palm | 2010 | `0x0040DC9C` | `0x004356D4` | `0x00411E60` | `0x00406CE8` | `0x00408FC4` | 9:30, 9:30, 9:30, 9:30 |
| 7 | One Eye | 2011 | `0x0040DDC3` | `0x004356EC` | `0x00411F9C` | `0x00406EAC` | `0x00409278` | 10:20, 11:0 |
| 8 | Raider | 2015 | `0x0040DE9A` | `0x00435704` | `0x00412098` | `0x00406F9C` | `0x00409530` | 12:30, 12:30 |
| 9 | Show Lin | 2018 | `0x0040DF71` | `0x0043571C` | `0x00412240` | `0x004070FC` | `0x004097E8` | 13:30, 13:15, 13:10, 14:20 |
| 10 | Dawg Cau | 2000 | `0x0040E55A` | `0x004357B4` | `0x00412848` | `0x00407EC4` | `0x0040A5E0` | 24:15, 24:15, 14:20 |
| 11 | Omoh | 2012 | `0x0040E197` | `0x00435750` | `0x00412470` | `0x00407930` | `0x00409D48` | 17:30, 17:30, 16:30 |
| 12 | Carmack | 2002 | `0x0040E296` | `0x0043576C` | `0x00412520` | `0x00407A6C` | `0x00409F64` | 18:30, 18:30, 18:30, 0:0 |
| 13 | Pain | 2013 | `0x0040E3BD` | `0x00435784` | `0x00412620` | `0x00407BE8` | `0x0040A188` | 19:20, 20:0 |
| 14 | Lo Pan | 2008 | `0x0040E483` | `0x0043579C` | `0x004126C0` | `0x00407CFC` | `0x0040A3B8` | 21:30, 21:30 |
| 15 | Mai Lai | 2020 | `0x0040E098` | `0x00435738` | `0x004123D8` | `0x004077F8` | `0x00409A9C` | 15:30, 15:30, 0:0 |
| 16 | Baka | 2009 | `0x0040E659` | `0x004357CC` | `0x00412778` | `0x00407FDC` | `0x0040A848` | 22:20, 23:20 |
