# Mission event queries

Use `mission_event_rect` for 460330 and `mission_event_linked_sides` for 460560. Keep the supplied `MissionTickState` alive. Both functions read its current actor records.

Set `MissionEventRect` to the signed side and inclusive X/Y bounds. The rectangle query checks group 3, then group 0. Zero side, equal bounds, and reversed bounds return zero. Inactive records participate. The side is the signed actor word at +72.

The linked-side query checks active group-3 records. Actor +90 holds a raw source address. Its target can be in any of the four current actor groups. The target needs no active flag. A reached address outside current storage raises `MissionTickError`.

The default count is the current table size. Set `current_count` when a host owns separate signed count cells. Negative counts skip the entry gate. The rectangle query reads a count at each group entry and after each failed record. The linked-side query reads the group-3 count twice and uses the second value as its loop bound.

`before_read` runs before each reached source cell read. Read observers can change current cells. The function then reads current storage. A count that reaches a record outside that storage raises `MissionTickError`.

Verification uses Df.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. The original comparison checks results and ordered read addresses in 520 cases. It also checks the original invalid-link fault. Native tests cover signed sides, inactive records, bounds, count changes, link changes, and missing storage.
