# Netsock ring queues

Use `MissionNetsockQueueBindings` with the same current DLL image and canonical memory backend as queue pop. Bind current IAT targets for malloc, free, and Sleep, plus diagnostic `10003AA0`. Keep all dynamic queue and payload bytes in that one backend.

`10003A60` allocates `10000` bytes, reads its current queue argument after malloc, clears all 16384 words, then sets the current base, head, tail, count, and lock fields. A null result reaches the source's first bad store after the queue-base write. Let the memory provider report the error at that point.

`10003B20` waits on the queue lock with `Sleep(0)`. Priority one drops a packet at signed count `800` hex. The general signed limit is `FF6` hex and reaches the diagnostic. Enqueue stores address, length, and port, frees the captured old payload, allocates the new payload, then reads its current source argument. Copy uses four-byte loads/stores and byte tail copies. Ring base and count stay current after callbacks and copies. Preserve wrapped arithmetic and the source write prefix when a required call fails.

`10003AD0` scans all 4096 physical slots with one cached free target, frees each nonzero payload, then frees the current queue base. It clears base, head, tail, and lock. It keeps the count field current. A zero base returns before imports.

The raw entry frames include a return cell. Init and close read queue at `+4`. Enqueue reads data, length, queue, address, port, and priority at `+4..+18`. Map eight bytes for init/close and `1C` hex bytes for enqueue.

Use the source worker lifecycle for live traffic. Host `10004A40` initializes receive `10019B0C` and send `10019B20`, calls `10002510/100021D0`, then starts worker `10004930`. That worker polls `10005830`, receives through `100052E0`, dispatches `100046C0`, drains `100048C0`, and calls its timed services. Those worker and OS bodies remain required. Client `10003A10` starts join worker `10003960` after clearing its work region; bind its reached queue setup separately.

The canonical memory provider must give all source threads current word values. Preserve source `Sleep(0)` and lock access order. Complete worker stop/join before destroying its queue memory. These queue bodies establish no separate packet ordering or retry policy.

Host worker `10004930` sleeps five milliseconds per loop. It reads `timeGetTime` through `10006088`, reaches `10004830` when unsigned current time exceeds the saved time plus 1000, and tests stop cell `10019B08`. Keep those source clock and stop reads when the worker is ported.

Evidence: `.local/reports/mission-netsock-queue-capstone-source.json` and the retained enqueue body in `netsocknl-native-evidence-bundle.json`. The original checker covers full clear/scan order, signed limits, wrap, aliases, callback mutations, allocation results, and error prefixes. Native tests join actual init, enqueue, GetMessage, queue pop, and close through one allocator and memory backend. Tests send no network traffic.
