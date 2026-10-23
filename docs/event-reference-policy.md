# Missing event references

Use `EventStage` with a real heap provider for strict source checks. Its raw trigger path forwards missing event IDs to `EventHost::external_event_fired`.

Use `event_fired_reference(stage, index, policy, diagnostic)` for an explicit reconstruction policy:

- Keep `strict` as the default. Missing IDs raise `EventError`.
- Select `missing_not_fired` to treat a missing ID as not fired.
- Supply a diagnostic that records the requested ID and loaded event count.
- Read valid IDs from the current event table. Valid reads keep their full low byte.

C05M07 contains 74 events and a reference to ID 74. The original reads beyond its event allocation. The safe policy repairs this asset error. It makes no claim about the original heap byte.

The reference gates dialogue 706 when one `glight` remains. The mission win event has separate count conditions. `MissionLogic` forwards the current host's missing-reference provider.
