# Model palettes

`PaletteQuantizer6` builds a palette from weighted RGB colors in gamma space.
Use `clear()`, add colors, then call `build()` for each bank. Keep the same
quantizer across banks. The original reset retains unused color boxes.

The histogram uses six bits per channel. Each bank includes grayscale values
0 through 254. Box splits minimize weighted variance. Palette entries use box
midpoints. The standard bank contains 254 entries.
Intel builds calculate variance with x87 precision and restore the caller's
control word. MSVC x64 uses `palette_variance_x64.asm` for this step.

Original code: `47EC10`, `47E660`, `47EDD0`, `47EF30`, `47FAB0`, `47FBB0`,
`47FD90`, `47FDD0`, `47FEB0`, `47ECD0` in `Df.exe`, SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

`tools/check_palette_quantizer_original.py` compares colors, histogram counts,
box bounds, moments, and variance with the original x86 code. The native test
checks fixed palettes and retained bank state.

`build_model_palette_banks32` uses models in loader order with explicit bank
numbers. It gathers selected textures and flat materials from all LODs, then
builds six banks and their shade tables. Texture selection uses the full stored
sequence and the selected level. A material with a missing texture uses its
flat color. Entry 0 is black; entry 255 is white, both mapped through gamma.

Original `47EAA0` includes all 256 source palette colors. `47E8E0` with flags 10
adds the first shade 31 times for each enabled shade path. Texture frequency
uses the original extended precision and truncation. The gather and bank
checkers compare this behavior with `47E8E0`, `47EAA0`, and `486670`.
