# Model drawing queues

Register each prepared material with `ModelDeferredBridge`. Keep its texture and palette storage alive until all queues drain. Preserve selector state across records and frames.

Set effect mode for the large model pass. Draw sky and terrain, cull near models, then call `draw_mixed`. Clear effect mode for near models. Draw primary water, `draw_no_context`, `draw_deferred_effect`, then companion water.

Span gradients use truncation. Half-blend colours use shade row 32. One-pixel spans use six fields; their four step fields have no effect on pixels.

```sh
python3 tools/check_renderer_deferred_original.py "/path/Df.exe" --probe build/renderer_deferred_probe
```

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `462DF0`, `4683B0`, `46BE50`, `46BF60`, `46BF10`, `46C4C0`, `46D190`, `485610`.
