# Netsock host discovery and admission

Use `mission_netsock_admission_call` with one current DLL image, packet buffer, address table and allocator.

| Entry | Body | Raw frame and arguments (hex) |
|---|---|---|
|10003F00|Discovery reply|10: packet4,IP8,portDWORDC|
|100040C0|Join admission|10: packet4,IP8,portDWORDC|
|10003ED0|Prior address cleanup|C: IP4,portDWORD8|
|10003F80|Join reply|14: statusWORD4,reasonWORD8,IPC,portDWORD10|
|10004090|Find or add address|C: IP4,portDWORD8|

Keep frames mapped through each call. `release` must not throw.4090 returns its current address pointer. Callers ignore the other bodies' return registers.

Discovery and admission compare four current application-key DWORDs. Keep each server-key read before the packet-key read. Discovery copies the current100-byte session to a1011 reply and clears its40-byte password field when flags bit8 is set. Read currentC5C before the cached reply's WORD length at send time.

Join admission checks current password flags and source byte order. It returns a1021 reply with status/reason2/2 for a key mismatch,2/1 for a password mismatch,2/3 for full address capacity, and1/0 after address lookup/allocation succeeds. Preserve the source's reached packet reads. The body has no opcode or packet-length check.

Join replies use current19B00+A0 as the host peer ID, orFFFFFFFF when19B00 is null. Create the host player through its source producer before using a real game peer. Keep cached IP/port arguments and current buffer, length and host-record reads at their source points.

Reuse the actual address lookup3FE0, allocation4050, removal4600, direct-send5240 and required5020 encoder provider. Keep all deterministic port DWORD bits at the call boundary. Bind required imports and codec services explicitly. Errors retain their reached stores and current port state.

The native test uses actualHostSession, an owned4930 worker, receiver, dispatcher, these bodies, and the encoder. It captures1011/1021 packets, checks rejection and capacity paths, and reuses source address rows. It closes during actualSleep5, runs source cleanup, then joins the thread outside memory locks. Its TCP-ready, buffers, endpoint, files and OS delegates are declared test inputs. It sends no outside packets and keeps the absent-player ID boundary explicit.

Source: Netsocknl.dll SHA256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. Exact bytes and instruction prefixes are retained in `.local/reports/mission-netsock-admission-scope.json`.

```
python tools/check_mission_netsock_admission_original.py GAME PROBE --report REPORT.json
```
