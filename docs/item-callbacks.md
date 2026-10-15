# Item binding

Call `bind_runtime_item_callbacks` after item parsing and before actor creation.
It uses the original AI and MOVE name tables. Empty and unknown names use row 0.
Callback addresses are lookup ids for native handlers.

Each item binds AI, MOVE, then four ambient names. Sound lookup results occupy
`118..124`; numeric ambient script values occupy `128..134`.

Source: `Df.exe` SHA-256
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
Entries: `4011C0`, `45C510`, `4563E0`. Original replay: 296 cases, zero differences.
The check records sound lookup calls and supplies fixed resource ids.
