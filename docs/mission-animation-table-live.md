# Current animation weight memory

Construct `MissionAnimationTableLive(runtime,cache,base,stride,capacity)` with live runtime/cache references. Reserve the full virtual span against other owners. Use an aligned nonzero base, an aligned stride of at least1C00h, and a positive capacity that fits the32-bit address space.

Use `table_address(current_definition.animation)` when a raw caller reads definition+78. Null returns0. Each known ANM handle receives a stable separate span. Keep captured table addresses tied to their assigned handle when a definition selects another ANM.

Route `claims` and `read` before fallback memory. Only each of256 rows' weight bytes at+10..+13 are exposed. Byte, word and DWORD reads must stay within one weight field. Unassigned spans, other fields, padding, overlaps and invalid widths raise errors.

Each read resolves the current runtime registry name and current cache ANM text. The codec uses the proved compiled action rows and ANM decoder to derive weight_10 without copying an action table. Loaded absent rows and unnamed slots have weight0. Null, removed, ambiguous or unloaded handles remain strict errors.

The native test covers stable handle spans, a current definition switch, captured old tables, null/hole reads, partial widths, range checks and current registry changes. Its optional game-folder argument checks all256 slots of the supplied C06M01 ANM tables against the authoritative action packer. Existing original packing proof is in `tools/check_animation_driver_original.py`; this memory adapter is a host interface.
