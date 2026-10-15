# Mission scene

`MissionScene32` loads six model palette banks, prepares model materials, and seeds CHR poses from frame 0. Keep its mission state, cache, and terrain alive.

Draw in this order: collect, queue large models, sky and terrain, cull nearby models, drain the mixed queue, draw nearby models and people, water, effect queues, then companion water.

Supply current actor images, CHR poses, weapon attachments, runtime actors, and effect records to `SceneModels32` for live frames. The game currently draws the initial mission scene. Runtime actors and effects have explicit input gaps.

```sh
build/scene_models_probe "/path/Delta Force" C02M01.BMS .local/scene.ppm
```

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Frame entry: `485610`. Collection: `485290`. Large pass: `485420`. Cull: `4854D0`. Near pass: `485580`.
