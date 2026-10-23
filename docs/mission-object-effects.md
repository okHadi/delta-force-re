# Object effects

Bind `MissionObjectEffectBindings` to current memory and synchronous required calls. The caller owns each frame until the body and all callbacks return. All frame offsets below are hexadecimal.

| Body | Entry | Local bytes | Arguments |
|---|---|---|---|
| Request | 446FB0 | packet00..11 | owner1C, pose20, count24 |
| Publish | 447610 | packet00..2A | owner38, X3C, Y40, Z44 |
| Apply | 4606B0 | pose00..17 | owner2C, pose30, count34 |
| Allocate | 460D20 | actor00..113 | owner120, X124, Y128, Z12C |

Keep apply-frame word14 as current caller data. The source writes the other five pose words. Keep source argument cells writable through callbacks. The request reads its pose pointer before the host gate and reads owner and count again after broadcast.

Route 442630 to the shared client outbox. Route 477850/477860 to the shared behavior RNG, 45DC70 to the wet effect, 4657D0 to the area queue, 43E7D0 to positional sound, 45FCF0 to the zone owner, and 463200 to the shared projectile ring. The two fragment loops use signed count shifted right once, pitch shifts15 and16, and speed multiplier20.

Allocation uses current FCE lookup and C23810 definitions. It returns the result of 45D970 mode0. This mode needs the primary group0 source-memory adapter, type6 definitions, current8CFA1C, and the person allocator when required by definition flags. Bind current raw definition pointers at this boundary.

The original comparison checks ordered memory access and calls, local bytes, argument cells, callback changes, packet fields, wrapping indexes, and failure prefixes. Foreign calls use explicit fixture contracts in this comparison. The native test uses the shared RNG, outbox, area queue, zone owner, and projectile ring. It checks the required primary insertion boundary after the exact image build.
