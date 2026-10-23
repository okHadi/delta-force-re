# Event actor orders

Use `mission_event_actor_orders` for45F670 and `mission_event_closest_marker` for45C5E0. Bind reads and writes to current memory. The canonical factory uses current mission actors, person records and marker lists.

Keep group3 and group0 counts current at each source read. Scan group3 first. Match the signed high word of actor70 when the requested side is greater than2. Read actor0C again before each person destination and at the helper entry. Preserve completed writes when a later destination has no owner.

Use source person addresses794680+slot*B4 in these bindings. Person slot0 is valid. Keep opaque typed handles separate from source addresses. Store A4 in `person_extra` and update its `tail_flag` alias.

Read group2 marker positions from current actor records. Read marker-list words from their contiguous88hex-byte records. Keep wrapped address calculations and valid aliases. Raise an error for a reached address without an owner.

Subtract positions with32-bit wrap, then use a signed right shift by16. Square and add with32-bit wrap. Compare signed distances. Select the later entry on a tie. Return the list-entry index.

Source: `.local/reports/mission-event-action-45f670-source.json` and `mission-event-action-45c5e0-source.json`, suppliedDf.exe SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

The original checker executes both bodies. It compares each read and write, current count/person/list/position changes and null-destination write prefixes. Run native sanitizer tests and Windows object checks.
