# DF network callbacks

`mission_df_network_callback` uses current `MissionClientAddedBindings` memory and required calls.

- `43FD60`: remove the client for the captured501344 peer. Map at leastE8 hex current local bytes. Preserve diagnostics, current client reads, departure text, actor removal, broadcasts, client clear and reference clear in source order.
- `43FEF0`: capture67DB40, get localized text through404CF0, then call required479F00 with the captured value.
- `43FF20`: write67DB68=1.

Keep the same client, actor and memory owners alive through each call. Local frame bytes remain current mapped input. The frame extent check establishes E8 bytes; the memory provider validates each reached read and store, including text writes. Foreign calls remain required when reached. Their errors retain the completed source prefix.

Use actual43FF30 callback registration and4B9400 current-pointer dispatch.1051 reads501378;1052 reads501380. Bind43FEF0 through its actual50137C caller. Keep479F00 required until its UI owner is bound.

The native test joins current MissionClientLive, MissionActorLifecycle,4423F0 broadcast, MissionClientOutbox and442450 reference clear. It checks captured peer/current pointers, actual actor and audio-registration removal, queued departure messages, callback changes and failure prefixes. The original checker compares ordered current reads, stores, calls and full client/local output bytes with the supplied executable. Tests send no packets and open no audio device.

Source: supplied Df.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Exact PE body ranges are43FD60..43FEEB,43FEF0..43FF12 and43FF20..43FF2A. Retained raw instruction inventory: `.local/reports/mission-network-live-callback-inventory.json`. Original instructions control read order, widths and wrapping arithmetic.
