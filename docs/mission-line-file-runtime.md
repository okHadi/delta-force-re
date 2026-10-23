# Loose text file backend

`MissionLineFileRuntime` reads files inside the supplied game folder. It binds `49A41C`, `49A550`, and `49A840` to a portable FILE owner. Shared memory holds the FILE fields, node, and 4096-byte buffer. FILE pointers are allocator-issued handles.

1. Keep the shared sound memory and backend bindings alive.
2. Supply aligned scratch of at least `0x500` bytes per nested parser call.
3. Use read mode `r` and line counts from 2 through `INT_MAX`. The stock parser uses 700.
4. Bind real recursive lock and unlock providers when a file is read.
5. Route `4568E0` through `service()` to use the existing parser and tokenizer.
6. Close each open handle through `49A840` before memory release.

The text reader consumes one byte after a carriage return. A Ctrl-Z byte reports end of file. Later reads can consume bytes that remain in the buffer. A final line without a newline remains unchanged; the existing parser's source error remains visible. Current pointer, count, flags, and descriptor cells control each read.

`448690` uses the existing banned-record body. `49AFA0` uses the shared integer reader after an independent original-instruction check. The actual missing `banned.txt` returns an open failure. A fixture with a real file checks parser, tokenizer, address records, and labels.

The portable FILE owner supplies regular loose-file input. CRT global file lists and lock implementations belong to the host boundary. Reached invalid-read diagnostics require `4B9C10`. Terminal input and other file modes require separate providers. DNS remains a required startup service.
