# Runtime items

`build_runtime_items` builds `0x13C` item records from `ITEMS.DEF`. Bind AI, MOVE, and sound names with `bind_runtime_item_callbacks`, then fill graphic, husk, CHR, and ANM handles through the resource plan.

Keep callback ids separate from native function pointers. Keep the four sound handles at `118..124` separate from numeric script values at `128..134`.

The original replay checks all 306 stock definitions, numeric boundaries, and the post-parse floating-point conversion.

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Load: `4827A0`. Count: `481BB0`. Parse: `481BE0`. Resources: `45D6F0`.
