# Cold graphic parent

`mission_model_load` implements `486370`. Pass an aligned argument block with destination graphic record, filename pointer and bank. Pass an aligned four-byte texture-count frame. Keep these bytes in the same required memory owner.

The destination needs its full current `7C` source load record. It includes partial file reads and overlapping word fields `70/72`. Use a source load-record owner for these writes. `MissionModelMemory` supplies a narrower view for current model users; its writable fields are the callback words.

Bind each reached service:

| Address | Service |
| --- | --- |
| `499040` | Current file existence check |
| `499120` | Current file open |
| `499280` | Current file read with its actual byte effects |
| `499020` | Current file close |
| `485BA0` | Texture load with the captured bank and shared texture counter |
| `485E30` | LOD allocation/fixup with current texture endpoints and captured bank |

Keep current texture counter word `C54480`, full returned LOD records and their material spans in the shared memory route. Material count/pointer fields are LOD `A8/AC`, stride `4C`.

The parent captures all three arguments at entry. It ignores open/read return values and checks the bytes that the read leaves in the current record. Valid magic is `3DI` and the version byte must be at least 5. Texture count is read again after each texture call. LOD count is read again after each LOD call. Preserve the signed count checks, zero returned LOD pointers and current metadata reads.

The file closes before the material metadata pass. This pass clears graphic `74`, then reads current first-LOD and material state. It stores the largest positive signed material duration for rates 1..7. It reads graphic `50` again before it copies first-LOD `24` to graphic `68`. A null first LOD leaves `68` at its current value. Parent success can contain null LOD pointers.

The original checker compares the full current header, frame, arguments, counters, LOD/material records and ordered parent calls/accesses. Its file, texture and LOD effects are explicit mutable providers.

The native asset test uses the real `MissionSoundRuntime` PFF/file/allocator owner for supplied Gazelle and `bexp` files. Its texture and LOD child providers consume the actual file spans and check cache headers, pixels and hardpoints. Those providers supply raw LOD pointer fields and source material bytes. Full texture registration, LOD mesh/material fixups, material FPU writes and bank side effects remain required production services. Keep the same geometry cache for those joins.

Evidence: `.local/reports/mission-model-codec-0x486370-source.json`, body `486370..4864E0`. Larger LOD dependency: `mission-model-codec-0x486040-source.json`, actual entry `485E30`. The checker validates the supplied executable SHA.
