# Model registry parent

`mission_model_registry_load` implements `486530`. Pass a stable aligned three-word argument block containing the current name pointer, bank and flag pointer. Pass a separate aligned 32-byte source scratch frame. Keep both in the same required memory owner as the registry.

Bind current count `C80D14` and source records at `C80D20`, stride `7C`. Record names start at `+4`. Keep one current registry owner. The module owns no count, names or model pool.

The parent clears the initial flag, searches current names, and returns an exact source record on a match. A new record needs count below 500. It copies the current input bytes in source order, adds the extension, enters the load context and calls the required cold loader. The count and success flag are written after the close call returns. A failed cold load returns zero and preserves its partial record writes.

Bind each reached service:

| Address | Service |
| --- | --- |
| `499E5E` | Source case-insensitive name comparison, through `49BE20` |
| `456560` | Source extension handling |
| `48FD00` | Load context enter |
| `486370` | Actual cold graphic load and its partial state |
| `48FD70` | Load context close |

The bank is read after context enter. The success flag pointer is read again after context close. Count is read after each failed comparison and before capacity, cold load and publication. Preserve these current reads when callbacks change state.

The source string copy has no fixed length gate. All writes go through the required memory owner. A copy beyond the declared scratch extent must fault there and preserve the written prefix. Keep the owner free of native buffer overflow.

The returned address is an original registry record. Convert it to the matching `MissionModelMemory` graphic address at the host service boundary. Require an exact known record and current registry membership. Keep the same cache and callback owner for that model. The cold loader stays required until its own file, texture, LOD, allocation and context services are joined.

The native test checks failed-load retry, case-insensitive cached lookup, current bank/flag reads, close failure, capacity and copy overflow. It runs actual `45D6F0` through this registry and the same model codec with supplied `bexp` data. Its cold loader is an explicit partial-state test provider. The original checker runs `486530`, `499E5E/49BE20` and `456560`; context and cold-load effects use mutable providers. The checker applies the declared memory-owner fault at a source copy overflow.

Evidence: `.local/reports/mission-model-codec-0x486530-source.json`, body `486530..486647`. The checker validates the supplied executable SHA.
