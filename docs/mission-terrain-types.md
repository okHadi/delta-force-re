# Terrain surface types

`mission_terrain_classify` ports `47B4A0`. Bind current byte reads for the raw color-map RGB palette and type rows at `BD4F98`, current count reads at `BD527C`, and byte writes at `BD5334`. The body publishes all 256 type bytes. Equal RGB distances keep the first row. Count zero selects type 1. Signed negative counts skip the scan and use row zero when the final count remains nonzero.

`mission_terrain_types` builds the table from `TerrainRuntime.color` and `config`. Use the raw color palette. `47AAA0` clears the initial row bytes. The `47AB00` `type` field has four values: red, green, blue, and surface name. Names are dirt (1), grass (2), snow (3), cement (4), sand (5), and packed_dirt (6). Matching ignores ASCII case. Integer conversion wraps to 32 bits, then stores a byte. Unknown names keep the current row type byte. The first 100 rows take part in classification.

Terrain marks read `TerrainRuntime.color.pixels[cell]`, then the current type table at that color index. Keep this table with the live terrain owner.

Retained source: `mission-terrain-type-producer-47b4a0.json`, `mission-terrain-config-line-47ab00.json`, and `mission-terrain-defaults-47aaa0.json` under `.local/reports`. The checker runs original `47AAA0`, full `47AB00`, and `47B4A0` from the supplied executable. It checks current-read changes, failed-write prefixes, config row limits, and actual C05M07 palette/config data.
