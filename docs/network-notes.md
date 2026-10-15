# Network code

Target: `Netsocknl.dll`, SHA-256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.

## Data path

- `netsock_SendToServer` at `0x10001dc0` builds an internal message and queues it.
- `netsock_SendToClient` at `0x10001cf0` uses the same message type, `0x1080`.
- These functions write the internal length at byte 2 and copy data at byte 8. The length includes 12 extra bytes; the remaining fields need separate checks.
- The queue writer is `0x10003b20`.
- The direct UDP sender is `0x10005020`. Its assembly shows three stack arguments: data, length, destination address.

## Direct datagram encoding

- Start CRC at `0xffffffff`.
- Use MSB-first polynomial `0x04c11db7`, with no final XOR.
- For 1–31 payload bytes, write the CRC as a four-byte little-endian prefix.
- For 32 or more bytes, save the original first 32 low bits in the prefix. Put CRC bit `i` into payload byte `i` bit 0.
- Empty input sends no datagram.

All 256 source CRC table entries match the generated polynomial table. The rebuilt encoder matches 468 original-x86 cases. Socket calls are stubbed during these checks; live network behavior requires separate tests.

REA evidence:

- Send API: `ev_704ba6ef85eefa3c85f7e09f87076cb66e3425f34c64239e41b0e64912fa25d6`.
- Queue: `ev_54ce4d673310863eba3ef33c9e35f1f42b3f7304867b463fc4dea1551d55fd2c`.
- UDP encoder: `ev_77a62626b5b2a699cd1df4303415921df722e01dd4c0e28e699a4ca8670d909e`.

```sh
.local/tools/python/bin/python tools/check_network.py "/path/to/Delta Force/Netsocknl.dll" build/network_probe
```

On a Windows multi-configuration build, use `build/Release/network_probe.exe`.
