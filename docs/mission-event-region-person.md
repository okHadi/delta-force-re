# Region person edit

`mission_event_region_person` runs `460280` with current `MissionEventPersonBindings` and a physical action address. Keep the action, group 3 actors, and person records in the same current memory owner. Bind scalar read/write and the x87 control provider used by reached person edits.

Action kind 12 selects class 2. Kind 13 selects class 1. A zero subtype at action `+8` returns. Other kinds return after the kind read. The function captures four DWORD bounds in this order: `+C`, `+14`, `+18`, `+1C`. They form X minimum/maximum and Y minimum/maximum for this function.

The scan starts at physical group 3 base `7CCA80`, stride `114`, with current signed count `8CFA18`. Each class check reads an unaligned DWORD at actor `+6E` and shifts it right by 16 with sign extension. This selects actor WORD `+70`. X and Y checks are inclusive and signed. Each matching actor goes through the existing actual `mission_event_person_edit` body at `45FF20`. The scan rereads the current group count after that call. Completed writes remain in place when a provider fails.

Supply raw bindings that read the actual action and current actor/person fields. The existing `mission_event_person_bindings` helper supports owned action arrays and canonical `MissionTickState` actors/people for native callers. Its physical person pointer conversion and lookup/x87 providers stay active at each reached source access.

The original comparison executes `460280`, `45FF20`, and `49BAE6` and checks full declared action, actor, and person bytes plus ordered width-specific reads and writes. The test uses source x87 control `037F`. Current count, action, actor class, and person pointer changes occur at declared memory callbacks. The native asset test converts supplied C05M07 group 3 records, then checks 37 physical actor/person edits through one canonical state. The separate child-module tests cover its other x87 controls.
