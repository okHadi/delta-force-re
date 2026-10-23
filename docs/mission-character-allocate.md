# Current character allocation

`MissionCharacterAllocateState` owns the source instance records at `42FC60` and occupied bytes at `439860`. Construct it from the exact 40,192-byte cold source image. Route its claims before the PE memory fallback.

The owner validates all cold bytes, including three nonzero source cells. Keep this owner alive while instance views are in use. Keep one animation controller identity joined to each reached raw instance.

`mission_character_allocate_raw` runs `439C70`. EDI supplies the loaded CHR payload. EAX returns the raw instance and EBX returns its slot. A full pool returns EAX zero and EBX 256.

Bind `439E48`, `43A0E0`, and `43A150` with their current register inputs and effects. These initialization bodies remain required.

`mission_character_allocate` runs `42CCE0`. It reads the supplied resource's `+60` pointer and uses eight shared local bytes below the supplied frame base. A null resource returns zero without memory or service calls.

The register contract includes EAX, EBX, ECX, EDX, ESI, EDI and EBP. ESP, EFLAGS and saved-stack slots are outside this interface.
