# Deferred spans

`DeferredPass` keeps the original record, node, and segment pools. Call `reset`, register materials, add faces, `flush`, then build or draw spans.

Gradients use the original truncation rule. Span construction uses perspective division in 32-pixel chunks. One-pixel step fields are unused by the pixel kernels. Frame rows contain pixel offsets.

`ModelDeferredBridge` supplies material bindings, selector state, effect routes, and pixel output for scene models.

```sh
python3 tools/check_effect_deferred_original.py "/path/Df.exe" --probe build/effect_deferred_probe
```

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `46C390`, `46C3E0`, `46C4C0`, `46CAC0`, `46CC30`, `46D190`. The replay checks pools, links, span fields, pixels, depth, and divide faults.
