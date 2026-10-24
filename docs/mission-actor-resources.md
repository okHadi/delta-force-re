# Actor resource cleanup

Use `MissionActorResourceBindings` with the current byte owners and required call services. The module owns no actor, controller, resource table, or allocation.

| Function | Source |
| --- | --- |
| `mission_character_release` | 42CD30 |
| `mission_character_release_raw` | 439C40; argument is source ESI |
| `mission_character_pool_clear` | 439C10 |
| `mission_character_resources_clear` | 42C950 |
| `mission_character_work_clear` | 42CF40 |
| `mission_actor_resources_clear` | 449620 |
| `mission_auxiliary_resources_clear` | 47EC50 |
| `mission_item_resource_links_clear` | 482910 |

Supply 4980D0 for allocation release, 49A620 for byte clear, and 42EBE0 for event cleanup. Keep these functions at their existing 404150 call sites. Provider errors stop at the reached operation. Release return values are ignored.

Use physical character addresses at 42FC60 with stride 9C. Route `MissionCharacterAllocateState` before the raw image. A host bridge maps typed animation handles to their physical records and keeps the typed controller lifetime current. Raw release reads instance+4, releases that allocation, then clears one occupied byte with the source signed index calculation.

Pool clear makes three DWORD passes. The final pass ends at 439B34. Supply the current adjacent bytes after the character owner ends at 439960. Keep every store in order.

Resource cleanup reads all 64 resource pointers at their source sites. Actor clear uses all nine physical clear spans, calls event cleanup, then copies current bytes from 4CC990. Auxiliary cleanup releases four pointers per row and clears five DWORDs after each row's calls. Item cleanup reads the current base before each field store and the current signed count after each row.

The native asset checks use one raw character owner and the real source arena. The actor image and event cleanup are declared host fixtures. Bind current canonical actors, persons, markers, items, event tables, and typed animation controllers before using the full chain in a mission.

The original checker compares all eight instruction bodies, full declared memory images, ordered memory/call hashes, callback changes, and failure prefixes. Separate original and native checks cover signed release indices at both 32-bit limits. CPU registers, flags, and saved stack bytes are outside this API.
