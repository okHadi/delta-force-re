# Weapon startup

`mission_weapon_startup` ports46BFB0..46C174. Map20 hex current local bytes and share the existing `MissionClientAddedBindings` memory owner.

The body publishes the original weapon, class and loadout pointers/counts, localizes weapon names, resolves current graphic item IDs and builds63 fire-rate words. Keep current AD3ED0 base reads after each localization/resolver call. Keep each current AD3EDC loop read, the later captured wrapping count*A8 bound and signed minimum comparisons. The source runs the first name iteration before its count test.

Bind493A80 for exact `STR_PWNAME%02i` and `STR_SPWNAME%02i` calls. Bind404CF0 to current localization. Startup4016F8 assigns `DFGAME.BIN` to503230; `DFMENU.BIN` belongs to503234. Bind4828C0 to the current runtime item-definition lookup. Each reached service is required. Keep the source table bytes mutable and preserve completed writes when a later call fails.

The native test checks declared current RTXT inputs and required-service errors. With a supplied game folder, it uses the actual PE tables, `DFGAME.BIN`, the real RTXT/localization bodies and current `ITEMS.DEF` definitions. Its two format providers are explicit native fixtures; the production format owner has a separate proof. No graphics IDs are guessed. Tests send no packets and open no audio device.

Original checks compare ordered reads, stores, calls, table/frame aliases, current callbacks, wrapping bounds and failure prefixes. Source Df.exe SHA256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. REA dossier: `.local/reports/mission-session-ready-46bfb0-producer-rea.json`. Text-file startup instructions: `.local/reports/mission-weapon-startup-localization-producer.json`.
