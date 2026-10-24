# DF host and player controls

Use `mission_df_host_control_call` with one current DF/DLL memory owner and required import providers.

|Entry|Body|Working frame, hex|
|---|---|---|
|4B91C0|Produce HostSession settings|130|
|4B92F0|Return the CloseSession import result|No local frame|
|4B9300|Produce the CreatePlayer record|A8|
|4B93D0|Destroy the current local peer|No local frame|

Map the full frames listed in the header. Keep caller-control bytes explicit and protect them from invalid local writes through the memory provider. CreatePlayer clears 88 bytes, while the reached DLL copies 98 bytes. The final 16 copied bytes come from caller-control cells 88..97. Supply the current calling context for these cells.

HostSession caches the password before its initialization gate. It reads the other arguments at their source points, copies all 16 current application-key bytes and reads the name after that copy. It sets `501368=1` after the required HostSession import returns and returns 0.

CreatePlayer captures its import flags before the clear. It reads the current name, user-data pointer and size after the clear. A nonzero import result returns 1. On success, it rereads the host argument and copies the current local peer ID to `501340` when that argument is nonzero.

Destroy reads the current local peer ID and returns 0 after the required import. CloseSession passes through the import result. Keep all reached imports required.

The native paired test uses declared caller-context bytes and checks the complete copied host token. It joins the real HostSession, CreatePlayer, DestroyPlayer and CloseSession bodies, source workers, encoded datagrams, free/event effects and external thread joins. TCP state, buffers, endpoints and absent optional files are declared test inputs. Each peer has one memory owner. Full DF player-added logic and the OS network backend remain separate integration boundaries. Test packets stay in the process.

The four DF imports use the supplied `Netsock.dll` export forwarders to the matching `Netsocknl.dll` exports.

Source: `Df.exe` SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Exact PE bytes and instruction replay define these bodies. The retained REA queries report missing function entries. The checker verifies repeat-copy prefixes, ordered current reads/writes/calls, caller bytes, failure prefixes and status results.
