# Water

Build `WaterScene32` from the prepared terrain. Supply the terrain frame's row-depth table and the scene clocks in `WaterPass32`.

Draw `render_water_pass32` after near models. Draw the no-context and effect queues, then `render_water_companion32`. Water changes colour and preserves depth words.

The water palette uses raw asset colours and the terrain colour controls. Camera bounds use an even left edge and odd right edge.

```sh
python3 tools/check_water_original.py "/path/Delta Force" build/water_probe
```

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `489440`, `489260`, `48D040`, `48D570`, `47C6A0`. The replay checks 23 wave cases, 6 colour tables, 12 row tables, and 20 water passes.
