# Current CRT thread

Keep one `MissionCrtThreadCells` owner for each current thread. Its seed starts at 1. Pass the same seed cell to `CrtRandomBindings::current_seed`.

Use `mission_crt_thread_bindings` to map source thread offsets `0x0C` and `0x10` into the shared sound and script memory. Supply a nonzero source address with eight mapped field bytes. Keep the cells alive through all callbacks. The adapter shares byte, word, and dword access, including access across both fields. Other addresses use the prior provider.

The tokenizer uses the current `+0x10` cursor. Keep its memory valid until the next tokenizer call changes it. Game owns these thread cells across mission changes.
