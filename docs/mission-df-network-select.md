# DF network selection

Use `mission_df_network_select_call` with the current DF cells, function tables and required providers in one memory owner.

|Entry|Body|Raw frame|
|---|---|---|
|497370|Select a transport table|8 bytes; selector at +4|
|4974A0|Close the current initialized transport|No local frame|
|4B8F20|Call TCP/IP initialization|8 bytes; descriptor pointer at +4|
|4B8F40|Call TCP/IP close|No local frame|

Selector 1 selects `501AB8`. Other values select `501A28`. When the selected table differs from `50139C`, the source closes the current initialized transport before it writes the cached selected table to `501398` and `50139C`. Cleanup reads the current table and its current +1C target. Keep that call required.

Selection and cleanup expose state only and return 0 through this API. The TCP/IP parents keep their defined status behavior. Initialization reads the pointer at descriptor +0, calls the required import, sets `501324=2` and returns 0. Close returns 1 when `501320` is zero. Otherwise it returns 0, calls the required close import when the protocol cell is nonzero and then clears that cell. Preserve ignored nested status results and required-call failure prefixes.

The native test joins the existing DF initialize/close bodies and DLL Startup/InitTCPIP/Close. It opens real UDP sockets, binds the test boundary to loopback, checks current OS buffer sizes, closes and opens the protocol again, changes the selected transport and checks all source frees. It sends no packets. POSIX uses no global Winsock startup or teardown. The codec library/export map is an explicit test provider for the existing compression body. Production OS and library providers stay required.

Compile with headers and a core archive from the same source revision. The new module owns no sockets, heap, queues or threads.

Source: `Df.exe` SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Exact PE instruction replay checks ordered current reads, writes, calls, callback changes, publication after cleanup and failure prefixes. The supplied `Netsock.dll` exports forward the TCP/IP imports to the corresponding `Netsocknl.dll` bodies.
