# Netsock host worker

Use `mission_netsock_host_worker` for the full blocking `10004930` thread entry. Map its 12 current local bytes: DWORD port at `0`, IP at `4`, and length at `8`. The source initially clears only port. Keep the current DLL memory, foreign buffers, allocator, receiver, and queue owner shared.

Bind source frames for the actual poll, receiver, and queue destructor. Use a four-byte poll local, the receiver's hex `48`-byte working frame with arguments at `34`, and a destructor entry of `8` bytes with queue at `4`. Release frames on success and failure.

The loop drains available packets, then calls outgoing work, service work, the current clock, periodic work, timer work, and cached `Sleep(5)`. It reads stop `10019B08` after sleep. Packet dispatch uses nonzero IP and the retained admit register. Port is a DWORD load from the local whose receive output is a WORD. A zero drop mask preserves the previous admit value. Keep the signed random shift and unsigned wrapping clock comparison.

Bind current import targets at `10006088` for time, `10006044` for random, `10006010` for sleep, and `10006048` for thread end. The source caches random and sleep after its first stop check. It reads the time target on each call. Keep packet dispatch `100046C0`, outgoing `100048C0`, service `100028C0`, periodic `10004830`, timer `100020D0`, and shutdown `10002D90` required.

On stop, the source destroys queues `10019B0C` and `10019B20`, calls shutdown, then ends the current thread. Supply actual DLL startup, socket/buffer producers, thread launch, stop, and join through the host. Retain source plain flag accesses and keep memory and socket lifetime valid for all workers. Production `_endthread` ends the OS thread. Controlled tests can return from that delegate and check its register value.

Run the original check with the supplied game folder and native probe:

```
python tools/check_mission_netsock_host_worker_original.py GAME PROBE --report REPORT
```

Evidence: `.local/reports/mission-netsock-worker-capstone-source.json`, body `10004930..10004A3D`, supported DLL SHA-256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
