# Mission actor and person state

`dfre::AiActorFactory` converts BMS object records into the original runtime actor records and person records. Rebuilt from original `449710` (record convert), `44A0C0` (person slot allocator), `44A110` (live-type count refresh), `487E10` (reset scan, thunk `488970`), and `449E62–449E98` (BMS group records to type slots). Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Build in group order 0–3. Rejected records stay as the zeroed 0x114-byte image the original leaves, but still occupy a loader slot. The group size counters are `8CF9A0/8CF9A4/8CF9A8/8CF9AC`. Every record is 0x114 bytes.

| Table | Base | Slots | Count |
|---|---|---|---|
| Group 0 | `7DDE80` | 1024 | `8CFA1C` |
| Group 1 | `867E80` | 1024 | `8CF904` |
| Group 2, markers | `8ACE80` | 512 | `8CF900` |
| Group 3 | `7CCA80` | 256 | `8CFA18` |
| Runtime extra, `0x487EA0` | `822E80` | 1024 | `8CFA14` |

The tables are contiguous in that order: group 3 ends at group 0, group 0 ends at the extra table (`45D970` stops its free-slot search at slot 0x3FF), the extra table ends at group 1, and group 1 ends at group 2.

Acceptance flags at record `+0C` (`449736–44979D`): bit `40` keeps the object only in single player; bits `10` and `80` only in multiplayer (`10` also needs byte `+45 <= rank`); bit `20` in multiplayer needs byte `+44 => rank`. Rank is original `67F530`, multiplayer flag `67F520`.

| Actor field | Value |
|---|---|
| `+00` | item definition index (lookup `4828C0`: first table record with id at `+20`, index 0 fallback) |
| `+04` | definition pointer (`C23810 + index*13C`); the native image stores the index |
| `+08` | BMS object ID |
| `+0C` | person slot pointer (`794680 + slot*B4`); the native image stores the slot |
| `+10` | graphics object pointer (written by `402EC0` from definition `+6C`; `487C50` reads `+64`/`+60` from it) |
| `+14` | husk pointer for groups 0/1 |
| `+18` | animation object for group 3 (gates the `487EA0` group 3 loop) |
| `+24/+28/+2C` | BMS position words |
| `+30` | `((90 - angle0) << 16) / 360 << 16`, wrap after each step, signed division |
| `+34` | `(angle1 << 16) / 360 << 16` |
| `+6C` | raw state byte (bit 2 skips update and list rebuild) |
| `+70/+72/+76` | BMS bytes `43`, `48`, `4C` |
| `+78` | countdown decremented by `487EA0` group 3 |
| `+7C` | only `1775` (value or `8000`), `177D` (byte `4A`), `1776` (value or `10000`) |
| `+FC` | animation counter % 60; counter starts at `4F821C` and adds 11 per accepted record |
| `+104/+108` | target-list start and count written by `487C50` |
| `+10C/+110` | definition `+90/+84`; callback slots called by `487EA0` as `(record)` and `(record, 0)` |

The person record exists when definition `+24` bit 2 is set (`449881`). The allocator takes the first slot whose first dword is zero (0x500 slots). Name text comes from the `People Names` RTXT entry `NAMEID<name_id:03d>` when the BMS name id at `+04` and the resource handle are present. Person flags at `+24/+25` map BMS flag bits `0C.0→1`, `0C.1→2`, `0C.2→8`, `0C.3→4`, `0D.0→10`, `0D.1→20`, `0D.2→+25.0`. `+74/+78` are `(x<<16)/100` (default `10000`); `+80/+84` are `x<<16` (defaults `100000`/`4000000`); `+7C` is `(byte 4B << 8)/360` (default `20`); `+98` is `(word 38 << 16)/360` with no trailing shift (default `8000`); `+88/+8C/+90/+94` are word `3A/3C/3E/40` times 62 (defaults `136`; `94` defaults `48A8`); `+54–60` copy the actor position and yaw; byte `49` sets `+A4=1` with bytes `49/4A` at `+AC/+B0`.

Person `+70` is the exact quotient `6553600/x` for BMS dword `+20` (x≠0, default `10000`, truncation toward zero). The original `449978–44999D` computes `FILD x; FDIV 100.0; FLD1; FDIVRP ST(1); FMUL 65536.0; FRNDINT truncate; FISTP`. The integer form equals the original for every int32 value: the extended-precision absolute error is below `1.5e-12`, less than the minimum non-zero fractional gap `1/2^31`, so divergence is possible only where `6553600` is an exact multiple of `x`, and all 57 divisors (both signs) were verified to return the exact quotient against original x86. Use the integer rule on Windows and Linux.

`refresh_live_type_counts` reproduces `44A110`: zero type slots `793680 + type*40` counts at `+28` and `+38`, count records in tables `867E80`, then `7CCA80`, then `7DDE80` by `(dword +70) >> 16`, then zero type 0 and copy each count to `+2C`. `load_type_slots` copies BMS group record words 0, 2, 3 into slot `+00/+30/+34`. `select_actor_reset_targets` reproduces `487E10`: visit group 3 then group 0, select records with a nonzero first dword and `(+6C & 2) == 0`.

Unknown and preserved: field meanings, the ITEMS.DEF table builder that fills `C23810` (fields `24/28/6C/70/74/78/84/90/94`), type slot consumers, and person pool overflow (the original writes through the null pointer). The `487EA0` tick and `487C50` list build are rebuilt in `ai_update.hpp`; see `ai-update.md`.

Run `ai_probe audit GAME_FOLDER` for a per-mission summary of real BMS data. Run `check_ai_original.py GAME_FOLDER AI_PROBE --report REPORT.json` for original x86 checks: all BMS records plus generated gate, angle, percent, special-id, and name cases for `449710`, plus `44A110`, `487E10`, and `449E62`, and the `449978` reverse-divide sequence over all 57 divisors, the ±20000 dense range, boundaries, and random values. The checker hooks `493A80`, `49A092`, `487C50`, and `499280`, executes `4828C0`, `44A0C0`, `49A620`, and `49BAE6` from original bytes, and uses the supplied executable read-only.
