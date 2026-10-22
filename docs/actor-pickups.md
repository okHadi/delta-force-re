# Actor pickup effects

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

- Bind actor views to canonical raw images, identities, original table addresses and optional person home fields.
- Bind the 17 client records and team counters to shared state. Keep callbacks on this same state.
- Use the pickup functions for `446F00`, `446F60`, `460470`, `4605D0`, `447220`, `4471D0`, `447280`, `4472D0`, `447360`, `446FB0` and `447610`.
- Execute every reached message, resupply, sound, removal, local flag, objective and allocation provider when called.
- Supply a live 16-byte pose to the objective helper. It copies its outgoing packet before the message provider and passes the current pose to `4606B0` after that provider returns.
- Supply actual allocation results to the spawn wrapper. It sends the original 43-byte packet for a returned actor.
- Use the defined `4605D0` return value for the restore-home query. Other pickup functions perform effects and writes.

The module owns packet layout, pickup flags, home position writes, heal scaling, delivery counters and client scans. Its synchronous foreign boundaries are `441750`, `447E80`, `43E720`, `484ED0`, `45DA30`, `4423F0`, `442630`, `4606B0` and `460D20`.

Run the signed native test and probe. Run `tools/check_actor_pickups_original.py GAME PROBE --report REPORT` with the local Python tools. The replay compares full actor images, client active/actor/counter fields, team counters, packet bytes and ordered effect snapshots. Fixtures include host-off paths and callback state changes.
