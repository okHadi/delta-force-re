# Mission periodic services

Use `MissionPeriodicBindings` for current source memory and synchronous calls. Keep source owners and each mapped frame valid through all callbacks. Missing reached providers throw.

| API | Source | Required frame |
| --- | --- | --- |
| `mission_periodic_scoreboard` | `4438C0` | `0x130` bytes |
| `mission_periodic_poll_clients` | `448C30` | None |
| `mission_periodic_clear_actor_flags` | `445C80` | None |
| `mission_periodic_advertise` | `446200` | `0x500` bytes |
| `mission_periodic_publish_flag` | `4482E0` | `0x20` bytes |

Use the existing round services through `mission_periodic_team_scores` (`446DA0`) and `mission_periodic_compare_clients` (`448130`). These adapters share current client and squad memory.

Route `4423F0` to the real round broadcast and `441750` to the shared client outbox. Route `49A620` to the current mapped memory clear. Advertisement requires current localization, formatting, address conversion, and network providers when its source gates pass.

Preserve caller bytes in the advertisement frame. Source branches can leave local text unchanged. The scoreboard uses its full frame for packet data and sort state. Callbacks can change these bytes and source globals before later reads.

The peer frame calls scoreboard and client polls after counter `6E473C` exceeds 310. It clears actor flag bit `0x40` after 496 and calls advertisement and flag publication after 1860. Cold counters first reach these paths on frames 311, 497, and 1861.

Original comparison uses the supplied `Df.exe` with SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. The checker compares current reads, writes, calls, raw frames, source globals, and failure prefixes. Native tests join canonical actor images, `MissionClientLive`, round broadcast, the client outbox, and single-player receive. The native message `15` handler is an explicit packet-check fixture; bind the live receiver for production use.
