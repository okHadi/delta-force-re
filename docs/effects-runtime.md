# Effect drawing

`route_effect_face` selects immediate drawing or one of four queues from face flags, material flags, effect mode, and draw enable. Drain queues in `485610` frame order.

`draw_effect_span32` implements the 32-bit alpha and half-blend kernels. Alpha uses one texel per pixel pair. Half-blend uses each pixel and clears the target high byte. Keep all four bytes of source colours.

Use `ModelDeferredBridge` to join these kernels to model drawing and palette banks. The selector keeps its previous kernel for keys with no case.

```sh
python3 tools/check_effects_original.py "/path/Df.exe" --probe build/effects_probe
```

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `46B1D0`, `4683B0`, `467FF0`, `4682A0`.
