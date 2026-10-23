# Actor message

`mission_actor_message_send` runs445CE0. It converts the raw actor pointer with unsigned32-bit subtraction and division by114hex. It sends the low index and message bytes as message25 through required4423F0.

Behavior actions supply the computed source quotient in their first argument. Pass that value to `mission_actor_message_send_index`. Keep its two-byte mutable payload alive through the synchronous broadcast call.

`mission_actor_message_receive` runs43F5A0. Supply current actor and definition reads, the current PE phrase table and real43E7D0 positional sound. It reads Z, Y, definition, phrase set and X in source order. It then reads the current message word and phrase table entry. Z adds10000hex with32-bit wrapping.

Bind the producer to `mission_round_broadcast` and the receiver to the live message25 call. Keep the canonical actor and sound owners current. Required missing reads, broadcast or sound report an error at that boundary.

Validation:356 original cases each with the normal and ASAN/UBSAN/no-elision probe, zero differences. Native ASAN checks join the real round broadcast, client outbox, local echo and live message25 receiver with current canonical actor reads and a declared sound fixture. Windows compilation passes for the module, probe and native test.
