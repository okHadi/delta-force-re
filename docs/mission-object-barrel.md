# Barrel actor

Use `mission_object_barrel` for 45BF80. Bind current memory, current stores, and synchronous required calls. Keep the caller's 24-byte pose frame mapped until all calls return.

`bind_mission_object_barrel` installs the update callback over the current physical actor record. The source ignores the message argument. The host flag 67F528 gates all actor access.

The body copies six current DWORDs from actor+24 in order. It calls 446FB0, reads current Z, Y, and X, calls 447610, then calls 4471D0. Bind removal to the shared actor lifecycle and client outbox. The 446FB0 and 447610 services remain required host boundaries.

The original comparison checks full actor and frame bytes, ordered reads and stores, callback changes, signed health, cached flags, ignored messages, and failure prefixes. The native test joins canonical actor storage to real removal and outbox publication. Foreign effect bodies use explicit test fixtures.
