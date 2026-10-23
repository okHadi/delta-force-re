# Event person fields

Route `EventHost::action_4601a0` to `mission_event_person_edits`. Build its
bindings from the current mission state and the current stable action record.
Supply the action source address, the current `4FB2CC` table owner, and the
current x87 control word.

The action selects a signed side from actor `+72`. The scan reads current
counts in group 1, group 3, then group 0 order. Each matched actor uses its
current person pointer. Person slot zero is valid. Actor activity does not
gate this scan.

`mission_event_person_edit` applies action subtypes 1–15. Writes update the
canonical person fields. Byte writes preserve adjacent bytes. Subtypes 7 and
12 read the destination person pointer again after the value read. Subtype 8
reads the value again after its nonzero gate.

The subtype 8 calculation divides the signed value by exact `100.0`, divides
`1.0` by that result, then multiplies by exact `65536.0`. Each division uses
the supplied x87 precision and rounding mode. The final result truncates to a
signed 32-bit integer. Use masked x87 exceptions and precision 24, 53, or 64.

Run `native_mission_event_person` for the real event-stage join and current
record checks. Run `tools/check_mission_event_person_original.py GAME PROBE`
for full-record, ordered memory, and original x87 comparisons.
