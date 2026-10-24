# Borrowed event memory

Keep `MissionEventResourceBindings`, its providers, and the actual memory owner alive longer than `MissionEventMemory`. Construct the adapter from a stable binding object. The adapter owns a lifetime token. Tables, counts, flags, and payload bytes stay on the existing source owner.

Use `event_at`, `trigger_at`, and `action_at` for a fresh record lookup. Each lookup reads the current count, then the current table base. Counts must be nonnegative, active bases must be nonzero, and complete table extents must fit the 32-bit address space.

Use record views to read or write captured fields with widths 1, 2, or 4. Views keep their logical address and 24-byte event or 32-byte trigger/action extent. Writes use the low bits for their width. Fresh table changes do not redirect a captured view. The actual owner supplies current bytes at that address after release, reuse, or reload.

Use `current_index` to decode a raw reference against the current table. Use `encoded_reference` to form a reference for a complete run. References must align with a record boundary and fit the current count. A zero-length run can use the one-past address when that address fits in 32 bits. `referenced_record` requires a complete active record.

`source_address` and `byte_size` return captured metadata. Memory access through a view requires a live adapter. Adapter destruction makes its views reject reads and writes before calling the provider. The host controls the longer lifetime of the binding and actual byte owner.

`load_current_file` and `reset` call the existing EventResources loader and reset. Supply the real current file, allocator, release, clear, and marker services. Use this same memory owner when the event engine is attached. Marker providers remain required.

The native test uses the published core and actual C05M07 BMS/PFF bytes. It checks field aliases, callback base/count changes, retained writes, same-address arena reuse, reload, reference limits, overflow, and adapter lifetime errors. Marker services are explicit test fixtures. This class is a host adapter; its checks make no claim of an original machine-code routine. The delegated loader/reset use the existing EventResources original comparison.
