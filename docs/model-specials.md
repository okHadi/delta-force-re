# Special models

Use `build_model_special_plan` for the 14 special rows in callback table `500C1C`. Plans contain mesh order, pivots, slot matrices, and ordered object writes. Supply the unscaled object matrix to the slot path. Supply the scaled matrix to draw rows that request scale.

`tcfe` and `ecaf` use a temporary camera-facing yaw while drawing. Fish use a temporary yaw byte. Tire meshes share their pivot angle. Tree slot pivots use the third callback argument.

```sh
python3 tools/check_model_specials_original.py "/path/Df.exe" build/model_specials_probe
```

The check runs the original sine-table initializer and compares all 2,561 entries. It compares draw matrices, all slot matrices, flags, and write values. Mesh drawing is a captured call in this check.

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `42A170`, `42A290`, `44C860`, `44C8F0`, `44D990`, `44DA20`, `480F40`, `4816D0`, and the remaining table rows.
