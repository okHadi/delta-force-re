# Netsock periodic and close services

Share one current DLL image, allocator, packet buffers, player/address tables, and queues. Keep raw frame bytes and cached source pointers alive through each call.

| Entries | Work |
| --- | --- |
| `10004830`, `10002D30`, `10002B80` | Age current rows and create player usage records |
| `10004820`, `10004600`, `10004490` | Remove expired addresses and linked players; queue notifications |
| `100042E0`, `10005010` | Build the reached player log call and resolve its IP text |
| `100020D0` | Send timed ping packets or start the client close handshake |
| `10001830` | Request client-worker close and wait for its actual acknowledgement |
| `10002D90` | Free the current usage allocation and clear its pointer |
| `10001880`, `100017F0`, `10003E20`, `10003E90` | Select close mode, send host notices, stop the host, and release players/tables |

Use the frame layouts in the header. Player usage caches its player pointer, then uses the caller's player argument cell as a current found flag. Keep DWORD and WORD counter reads, wrap, record copy/clear widths, current counts, and cached table bases at their source points.

Normalize only `10003E20 → 10005240` argument 4 to its consumed WORD. Its other bits belong to a caller-saved register. Preserve the deterministic ping port high 16 bits from the cached timestamp. Keep every real memory field unchanged in width.

Bind reached clock, allocator, queue, send, time/log, and OS services. Client close caches `Sleep`, clears `19C14`, writes its request, and waits for the client worker to set `19C14`. Keep that acknowledgement producer required. Host close writes `19B08` before removing players and freeing tables. Preserve this source order. The OS host owns thread scheduling, safe table lifetime, and join outside these bodies.

The native test uses an owned host thread and thread-safe memory. Its Sleep provider performs a real sleep, then holds a source-valid schedule while the main thread removes a current sample player and calls source `CloseSession`. The test releases the worker to observe stop, clear queues, free usage, and exit; the host then joins. A separate owned thread produces the client acknowledgement. Tests check queued notifications, payload release, usage state, ping bytes, and required-service failure prefixes. Their send provider captures packets; OS packet output is zero.

The original checker compares ordered reads, stores, calls, state, and meaningful returns. Its retained instruction tape records DLL and checker hashes for sanitizer replay. Source evidence: `.local/reports/mission-netsock-periodic-scope.json`. DLL SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
