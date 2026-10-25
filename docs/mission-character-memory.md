# Character memory view

`MissionCharacterMemory` borrows one `MissionCharacterIdentity`, its current publication metadata, the same controller lookup, raw memory, and current arena allocation identities. Keep these owners alive and stable during each call. Route backing reads and writes directly to their owner.

Publish after complete source setup and the initial-state check. Enumerate each publication with its captured token, physical row, payload, and allocation incarnation. An expired allocation uses raw backing. A new allocation needs its own explicit publication.

The view owns current scalar fields18..2C,34..40,48..50,60,64..66,78..7B. It supplies read-only current animation/frame pointers0C/10 and bone count14. Pose80, delta84, and root8C/90 arrays use the existing controller storage. Array pointers must match the439E48 allocation layout and current bounds. Get a fresh source view on every access.

Use widths1,2,4 within one scalar or array component. Byte fields use width1. Cross-field accesses and derived-field writes fail before mutation. The raw owner keeps instance00/04, allocation headers, occupied bytes, array pointer cells, and other fields.

Call `handoff(token)` before the real release. It validates current publication, allocation and array layout, then writes reviewed scalar/derived cells in source-offset order, followed by pose, delta and both root arrays. Keep typed state and owners stable. Backing stores can throw before a rejected write; earlier writes stay committed. The method keeps the controller and Identity publication alive.

Run the real release after a successful handoff, then remove Identity metadata and release the typed owner. The heap release callback reads final bytes after unlink. Raw backing retains these bytes until a later source write or allocation reuse.

The native test uses actual PLAYER01.CHR, source resource preparation and arena allocation, full instance setup, publication, partial writes, typed ticks, renderer pose reads, scalar/array handoff failure prefixes, real heap callback reads after unlink, source release, and same-address reuse. Its fixed virtual addresses and deliberate field changes are test inputs. Device output stays silent.
