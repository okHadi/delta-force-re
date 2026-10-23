# Game transport bridge

Use `MissionGameTransportBindings` with one current memory provider. The wrappers read the current `501398` table and its function word on each call:

- `497790`: receive at `+54`.
- `4977A0`: send to server at `+58`.
- `4977E0`: send to client at `+60`.

The matching Netsock bodies are `4B9530`, `4B95D0`, and `4B96B0`. Other selected targets require the host's `call` provider. Keep the source transport selection current.

Bind these synchronous calls:

- `4C551A`: DLL GetMessage, with a mapped four-byte length cell and current sender cell `501344`.
- `4C5514` / `4C550E`: DLL SendToServer / SendToClient.
- `4B9400`: dispatch a received system message before the next receive.
- `490880`: pump the real host during send retries.
- `4C5562` / `4C555C`: DLL Init / Close.
- `4983B0` / `498400`: allocate / free the shared receive buffer.
- `4B8DA0`: remove the current registered connection during close.

Map and release each reached receive frame. The source reads `501320` before it stores local capacity `8000`. After GetMessage, it publishes the current `D0A540` and returned local length to `50135C/501360`, including on error. A successful receive captures the counter, reads the sender, then stores the incremented counter. Sender zero dispatches the system message and starts a new receive frame. All frames release when a required body fails.

Send retries use DLL error `10` hex. Each retry increments current `501364`, pumps the host, then reads the current counter. A value above `800` hex returns two. Success returns zero; another error returns one.

`4B8EC0` calls DLL Init, sets `501320`, and allocates `8000` bytes at `D0A540`. Preserve a returned null pointer as the source does. The host allocator reports its own errors. `4B8EF0/4B8E10` drain current connection heads, close the DLL, free the current buffer, then clear its pointer and enable cell.

Keep game-message local echo in the shared `GameMessageStage`. `MissionClientOutbox` uses that echo when the peer matches current `501340`. `460E40` consumes its shared peer buffer before testing `67F520`; zero skips network receive. Remote paths call this bridge and publish the same parser cells.

The DLL packet queues, workers, wire codec, and OS socket calls are required external bodies. Use their canonical Netsock owner. DF `454BB0` status sockets have separate source cells and a separate purpose.

Source: supplied `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Retain REA evidence in `.local/reports/mission-transport-*-audit.json`, exact PE ranges in `mission-transport-missing-producers-capstone-audit.json` and `mission-transport-df-wrappers-capstone-audit.json`, and close-loop evidence in `mission-game-transport-close-helper-source.json`.

Run `check_mission_game_transport_original.py` with the supplied game folder, probe path, and `--report`. It checks source read/store/call order, current table and buffer changes, recursive receive, retry limits, allocation results, and failure prefixes. Native joins cover the actual local echo and peer receive gate, plus remote bridge boundaries. Tests use explicit DLL fixtures and send no network traffic.
