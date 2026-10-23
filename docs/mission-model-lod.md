# Model LOD loader

`mission_model_lod_load` ports `485E30..486366` from the supplied `Df.exe`.

Provide one current raw memory owner, a 12-byte argument block and an aligned 40-byte scratch frame. The arguments hold texture start, texture end and bank DWORDs. Keep their bytes current through calls. The scratch frame contains source spills and the x87 result.

Bind these reached services:

- `499280(destination,size)`: read from the current model file.
- `497F60(size,20h,0,record)`: allocate the payload.
- `material_ticks(material,result)`: execute source `4860BE..4860D6`. Read the current signed material word at `1C`, double constants at `4CDC80/4CDC78`, and current x87 control/status. Apply the divide, multiply, `49BAE6` and result store through the same memory owner.

The parent reads the result DWORD and stores its low word to the captured material. A host with the proved fixed constants `100.0/62.0`, supported x87 precision and rounding, and a declared numeric-only contract can use signed integer `word*31/50` with truncation toward zero. Validate that contract at the service boundary. Bind other FP state contracts to an exact provider.

Keep one current LOD registry at `C65DD0`, stride `B8`, with count `C65DCC`. Keep texture descriptors at `C54488`, stride `24`, and the current texture/localization gates at `C80D10/C80D18`. File, allocation, registry and texture storage belong to the host.

The parent publishes payload pointers, four mesh pointer passes, face material pointers, material bank/texture fields, localized vertices and mesh delta origins. It reads current counts and bases at their source sites. A reached missing service raises an error. Provider errors retain the completed write prefix. The final count store follows all reached stages.

Run `native_mission_model_lod` with the game folder to check supplied Gazelle and bexp files through the real file/allocator owner and actual `486370` parent. The test supplies texture descriptor loading explicitly. It compares cache counts, localized vertices, hardpoints and complete file consumption.

Run `tools/check_mission_model_lod_original.py GAME_FOLDER PROBE` for original instruction comparisons. The replay executes the actual x87 slice and records it as one explicit service boundary. It compares raw LOD/payload/frame/argument/texture state and ordered parent accesses. File and allocation effects are declared fixtures. CPU register, stack-save and FP status equivalence require separate host contracts.
