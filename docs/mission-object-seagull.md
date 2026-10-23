# Seagull callbacks

`mission_object_seagull_ai` implements `451FD0`. `mission_object_seagull_move` implements `452310`. The two flock helpers implement `44FD30` and `44FD80`.

Bind reads and writes to the current physical actors, definitions, mission gate `67F528`, and a stable aligned 28-byte AI frame. Keep frame bytes current through callbacks. The AI uses offsets `0`, `4`, `8`, `C`, `10`, `14` and `18`.

The flock helpers scan all 1,024 physical group-0 slots at `7DDE80`, stride `114`. Each match needs a nonzero item index, clear dead bit `6C:4`, equal word `72`, and equal item ID at the current definition pointer plus `20`. The first helper returns the first match or zero. The index helper starts at `FFFFFFFF`, counts matching rows, and returns when it finds the current actor. Supply the real current tail records for this fixed scan.

Bind each reached service:

| Address | Service |
| --- | --- |
| `477860` | Advance the shared behavior random state. |
| `477820` | Write sine and cosine to the supplied current frame addresses. |
| `477670` | Compute the source approximate 3D length. |
| `477770` | Compute the source angle. |
| `47DF40` | Query current terrain height. |
| `43E7D0` | Run the current positional sound request with device output off. |

`bind_mission_object_seagull` registers both callbacks. Its actor references use physical source addresses. Keep the bindings and frame owner alive for the mission.

The native test uses one `MissionTickState` and `MissionPrimaryActorLive`, including explicit cold tail images. It checks current flock flags, shared RNG, sound-call mutations, canonical movement, terrain bounce and partial state after errors. Its sound service is an explicit test provider. The original checker executes both callbacks, both flock helpers, the real math and random bodies, and `477510` to initialize the sine tables. It compares current memory and the ordered parent reads, writes and foreign calls.

Exact PE evidence: `.local/reports/mission-seagull-source-bytes.json`. Body ranges: `451FD0..452300`, `452310..4525C8`, `44FD30..44FD75`, `44FD80..44FDD4`. The checker validates the supplied executable SHA.
