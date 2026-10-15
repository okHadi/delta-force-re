# Item definitions

Read `ITEMS.DEF` from the supplied archives. Decode with original `456EB0`: reverse all bytes, then XOR each byte with the low byte of the next key. Start the key at `04960552`; advance it with `rol32(key + rol32(key, 11), 4) ^ 1`. Startup `401050` sets this key.

Read `begin NAME` through `end` records. Preserve each field and its order, including repeated `attrib:` fields. The stock file has 306 records. Fields bind graphics, husks, character files, animations, health, movement, and AI callbacks.

Keep the source ID. Subtract 100,000 with 32-bit wrap for the runtime ID, as `481D16` does. Match BMS object type IDs against runtime IDs. Use the first item for a missing ID, as `4828C0` does. The 62 supplied missions contain 25,492 objects; 25,490 IDs match a defined item and two use the original fallback.

`items_probe GAME_FOLDER` checks the catalog and mission bindings. `check_scripts_original.py GAME_FOLDER SCRIPT_PROBE` compares decoded bytes with the original x86 transform. Keep raw scripts in `.local/`.

Source executable SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Format reference: [NovaResearch SCR](https://github.com/Novahq-net/NovaResearch/blob/main/Shared/SCR%20File%20Format.md). Original bytes and replay checks establish the DF1 behavior above.
