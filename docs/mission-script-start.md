# Script start and queue state

Use one `MissionScriptQueueCells` owner for `MissionScriptStart`,
`MissionFrameServices`, and `MissionScriptRuntime`. Initialize the queue to the
state cleared by `455C90`: all 16 physical cells and the count are zero.

The production vector holds the counted, nonzero resource prefix. Unused
physical cells are zero. Start writes resource, index zero, and delay zero.
It retains the unused cell's armed field, which is zero in this state.
Removal clears a full cell and compacts full 16-byte cells. These operations
preserve zero unused cells, including cell 15. Use nonzero resources for the
production start API.

Supply current registry-head and node-word providers. The comparison provider
executes one `49AD30` boundary. Its right operand is an explicitly tagged raw
name address or a temporary text view. Keep the view within its call.
The resource is captured before comparison. A failed match reads the current
next link after comparison.

`event(id)` uses `dlg%03i` for nonzero signed IDs. Its 32-byte local buffer holds
every signed 32-bit ID. ID zero returns before any registry access.

The raw queue API reads all 16 physical cells. It preserves an arbitrary
unused armed field. Supply current reads, writes, comparison, and full-cell
clear providers for that API.

Run `native_mission_script_start` for the shared start, frame-service, and
script-runtime join. Run `tools/check_mission_script_start_original.py GAME
PROBE` for original queue, registry, formatter, mutation, and repeated reuse
checks.
