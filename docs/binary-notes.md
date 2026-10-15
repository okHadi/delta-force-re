# Binary notes

## Df.exe

- SHA256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`
- Size: 1,102,860 bytes. PE32, x86. Image base: `0x400000`.
- Entry: RVA `0xA1258`, VA `0x4A1258`.
- Observed label: `WATCOM C/C++32 Run-Time system`, file offset `0xA0661`, VA `0x4A1261`.
- Compiler inference: Watcom C/C++ family. Exact version is unknown. Linker header: `2.18`.
- All six section headers have `VirtualSize=0`.
- `.bss` has uninitialized-data flags, raw pointer `0`, and declared raw size `8,688,128`. Treat it as memory, with no file bytes.
- REA 6.5.0 resource inspection rejects this `.bss` range. Its native target parser accepts this file as an x86 PE.

Imports: `ole32.dll`, `WINMM.dll`, `GDI32.dll`, `ADVAPI32.dll`, `USER32.dll`, `DPLAYX.dll`, `KERNEL32.dll`, `DDRAW.dll`, `DSOUND.dll`, `netsock.dll`, `WSOCK32.dll`.

Display uses `DirectDrawCreate` and GDI drawing imports. Sound uses DirectSound and WinMM. Network imports include DirectPlay, Winsock, and the netsock API.

## Network DLLs

- `Netsock.dll` SHA256: `fba507ef8dd7a01511082077c072d860765b0b73642bcfd05dd53f246634a4f7`.
- Its version fields identify NovaHQ Heartbeat DLL `1.0.1.0`, copyright 2017. All 28 exports forward to `Netsocknl.dll`.
- `Netsocknl.dll` SHA256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
- `Netsocknl.dll` holds the network API behind the wrapper. Its original release identity is unverified.
- It imports `KERNEL32.dll`, `WSOCK32.dll`, `WINMM.dll`, and `MSVCRT.dll`. Winsock calls include `socket`, `bind`, `recvfrom`, and `sendto`.
- Compiler inference for `Netsocknl.dll`: MSVC family, based on linker `6.0` and MSVCRT imports.

## Analysis seeds

Use references to these addresses to find functions. Function names and boundaries need code analysis.

| Item | Df.exe VA |
| --- | --- |
| `df.pff` | `0x4C8080` |
| `dfbase.pff` | `0x4C8088` |
| `C%02iM%02i.BMS` | `0x4C8E14` |
| `PFF STRUCT` / `PFF HEADERS` | `0x4CE468` / `0x4CE474` |
| PFF read error / `pffmgr` | `0x4CE490` / `0x4CE4B4` |
| `DirectDrawCreate` import slot | `0x4C6874` |
| `netsock_Init` / `netsock_InitTCPIP` slots | `0x4C68BC` / `0x4C68C0` |
| `netsock_SendToServer` slot | `0x4C68CC` |
