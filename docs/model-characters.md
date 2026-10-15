# Character models

`build_character_draw_plan` joins the current CHR pose, actor transform, mesh origins, and caller-resolved weapon attachments. Draw its items in order. Preserve the attachment counter and material-frame writes.

Supply one pose per character instance. Use the actual weapon record type and LOD count for attachments. The character matrix table also supplies attachment mount slots 1 and 7.

The original replay checks actor meshes, attachments, pitch, counters, and all 2,561 sine-table entries. It runs the original table initializer with FPU control word `037F`.

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Matrix callback: `481130`. Draw callback: `481330`.
