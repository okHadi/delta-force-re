# Mission host pump

Use `mission_host_pump` for the 490880 message loop. Remove each message, return
on WM_QUIT (0x12), and translate then dispatch each other message. After the queue
is empty, run CD, streamed PCM, and MIDI upkeep in that order.

Use `mission_host_upkeep` once after a platform message drain with no quit event.
`GameWindow::poll` supplies the normal dispatch stage and returns input events.
Its Win32 and SDL loops continue after a quit event. Exact 490880 integration
requires the platform drain to return at that event. The pump's nested upkeep
functions manage audio; normal input enters through message dispatch.

## Shared state and services

Bind `word`, `store`, `byte`, and `store_byte` to current shared storage. Source
addresses identify cells and buffers. Store portable handles in pointer cells
and map those handles at the host boundary. Keep the input buffer D00930 through
D0892F and the 16 PCM slots CC0930 through D0092F available.

Bind each reached service. A missing binding raises `MissionHostPumpError`.

| Binding or call site | Required work |
| --- | --- |
| `peek_remove`, `translate`, `dispatch` | Remove and process real window messages |
| `read_file` | Read the current file into shared PCM input bytes |
| `seek_file` | Seek the current file to absolute byte 44 on a short read |
| Current callback at 5012E4 | Run the CD repeat callback with the current track |
| 493CD0 | Start CD playback and update CD state and deadline |
| 4B5243 | Queue PCM using ESI buffer, ECX size, and EBX zero inputs |
| 49EFD0, 49EF20 | Stop and restart MIDI, including source state updates |
| `timer_caps` | Supply the actual timer capability output words |
| 4C658C | Call the `timeBeginPeriod` import |
| 4C65A0 | Call the `timeSetEvent` import with its five arguments |

The CD playback, PCM backend, and MIDI stop/restart bodies are external services
for this module. Supply their full state effects. Bind muted device output while
keeping file reads, buffer queues, playback state, and timer work active. Keep the
system volume unchanged.

CD upkeep uses a strict unsigned deadline comparison. It reads the repeat and
track again after the callback. PCM upkeep reads current format and ring state
after file callbacks. It converts 16-bit signed samples to unsigned 8-bit samples.
The 8-bit path copies complete DWORDs and retains the last one to three slot
bytes. MIDI upkeep uses a wrapping unsigned elapsed time greater than 0x960 on
the nominal 120 Hz clock, then clears the current shared notification flag.

## Timer

Bind 491014 to `MissionTickState::clock_491014`. Keep 49101C:491020 and their
increments in the same shared clock state. `mission_host_clock_callback` ports
4906A0 with wrapping addition and carry. It runs when CC0884 or CC08BC is set.

`mission_host_clock_configure` ports 490750. Supply the current CC0868 interval,
49102C user value, and 491018 primary increment. Source startup clears the timer
cells at 49100C through 491033, sets CC0884 and 491018 to one, and obtains 49102C
from `timeGetTime`. Full startup also calls 497970 outside this module.

Use a real periodic timer for asynchronous delivery, or use
`MissionHostClockDelivery` with the installed interval and real wrapping
millisecond samples from `GameWindow::ticks`. Its first sample sets the anchor;
later samples deliver each elapsed interval and retain the remainder. Sample
while inactive to apply the source gates to elapsed intervals. This delivery
policy defines callback timing; WinMM scheduling and jitter require the OS timer.
The zero CC0868 setup path installs 25 ms and retains existing secondary clock
increments. The nonzero path computes increments from the current interval after
the timer call. A timer ID of zero remains visible to the host.

## Checks

The executable SHA-256 is
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
REA source is retained in `.local/reports/mission-frame-services-retained-root.json`,
`mission-host-pump-nested-source.json`, and `mission-host-pump-clock-source.json`.
The clock callback body is also checked directly from its original instructions.

Run `tools/check_mission_host_pump_original.py GAME PROBE --report REPORT`.
The check runs seven original bodies with explicit foreign service fixtures. It
compares all selected cells, the CRC32 of the whole PCM input/ring storage, and
ordered calls under state changes. The PCM pair-loop guard stops at its actual
loaded count when the original loop cannot terminate within its buffers. Native
code raises an error there. Signed timer division overflow also raises an error.
Native tests cover missing services, PCM tail bytes, current state reads, timer
wrap, inactive periods, and fractional delivery. These checks cover this module;
real window, file, device, and timer integration require their host bindings.
