# Client readiness

Use the current `MissionClientAddedBindings` memory and required service calls.

- `mission_client_ready`:446A30. Map128 hex bytes. Slot is at120; packet pointer is at124. Source locals occupy10C bytes. The source clears only the firstE2 bytes. Keep all frame bytes and argument rereads current.
- `mission_client_resupply`:447E80. Pass the current raw client pointer and current weapon/ammo tables.
- `mission_client_reject`:447FB0. Map4C hex bytes. Local text occupies40 bytes; slot is at48.

Readiness comes from the actual message11 handler. It writes client1C4 WORD5 before later work, clears actor6C bit02, copies input strings, builds theE2-byte reply, enqueues message0B and restores ammo. Preserve completed writes when a later service fails.

Keep current actor/person images, marker lists, clients and weapon tables in their canonical owners. Read marker counts as signedWORD values and clamp only values greater than8. Preserve the yaw/count overlap in the localDWORD at46. Use the source88-hex marker-list stride and128-record scan through the memory provider. Each reached text read/store needs valid backing.

Resupply uses ten current inventory words and an optional three-word secondary pass. Keep each weapon-kind reread and wrappingDWORD product. The final client154 value is1000.

Read all nine content values atBD67D4..BD67F4 and the corresponding packetBA..DA words in source order. A mismatch for a nonzero current slot reaches the real reject body. Reject enqueues message44, localizes the text, formats `{destination,4CC278,text,client_name}`, then reads current67DB40 for required479F00.

Bind441750 to the real outbox. Bind current fill, format and localization services only for supported source calls. Keep479F00 required. The native test supplies declared startup client, actor/person, marker, weapon and content tables. It enters the actual peer parser message11 handler, then message10 with source-produced readiness and current terrain. This is a component join; runtime startup and network transport have separate owners. Tests send no packets and open no audio device.

Original checks compare complete ordered reads, stores, calls, caller-frame aliases and failure prefixes. The supplied Df.exe SHA256 is `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Complete source bodies are446A30..446D92,447E80..447F5F and447FB0..44803E. REA dossiers are retained as `.local/reports/mission-client-ready-*-rea.json`.
