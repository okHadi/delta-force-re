# Behavior zones

Bind `45FCF0` to `mission_behavior_zone_add` and `45FD80` to
`mission_behavior_zones_reset`. Keep count `4F8D44` and records `8E48B0`
with one current owner. Records have a 32-byte stride. Read and write widths
are four bytes.

The host gate is `67F528`. The source cap uses a signed count below 200.
All coordinate arithmetic wraps at 32 bits. The source writes minimum Z,
then count, then maximum Z. Reset clears count and keeps record bytes.
Expose the active records to the existing actor query provider at each query.
Use `MissionBehaviorZones` as the record owner. Its bindings read the current
host gate by reference. `active_records()` reads the current active records.

Run `check_mission_behavior_zones_original.py GAME PROBE --report FILE`
for original instruction comparisons. Audio output stays muted.

Source `Df.exe` SHA-256:
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
