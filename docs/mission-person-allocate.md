# Current person allocation

`mission_person_allocate` runs `44A0C0`. Bind the canonical 1,280-person pool at `794680`, with stride `B4` hex.

The function selects the first zero owner, calls `49A620(person,0,B4)`, then writes the requested owner. It returns the physical person pointer. A full pool returns zero.

Use the same memory bindings as `mission_actor_create`. Clear the current person and its extra fields through the shared clear provider. Keep callback writes current when an error occurs.
