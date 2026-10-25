# Debrief format service

Route `493A80` from the Debrief bodies through `mission_debrief_format`. Supply current byte reads and writes with `MissionDebriefBindings`. Keep captured scalar arguments stable through the call.

The service accepts the fourteen source literals for campaign filenames, Win/Lose/Briefing keys, missing-text errors, statistics, waypoint keys, Ready text, and player-weapon keys. Keep their exact PE bytes and NUL terminators immutable. Literal validation reads must have no side effects. Unknown pointers, changed literals, wrong argument counts and output writes into literal spans throw.

String arguments use current reads. The service scans each string for its length, then reads and writes each captured byte in order. A null string is empty. Supported string/output aliases retain this order. Each output byte and the final NUL use a separate width-one write. A failed provider leaves earlier writes committed. The return value excludes the final NUL. Signed decimal conversion includes `INT_MIN`; campaign filenames and player-weapon keys use source width-two zero padding; waypoint keys use width-three zero padding.

Run `native_mission_debrief_format` for guards, current string changes, overlap and write-failure prefixes. Supply the game folder to test all 80 campaign texts and 18 terminal `{ADD}` references through real RTXT, heap, profile, Text and Debrief bodies. The same run uses real weapon startup for39rows and78 localized names. Menu, display, save and `4C4F40` calls use declared test providers. Device output stays zero.

Run `check_mission_debrief_format_original.py GAME PROBE --report FILE` to compare full original CRT execution, current string reads, output writes, return values and mapped-memory failure prefixes. The comparison excludes the extra pure literal validation reads. It uses stable original argument frames, as required by the public captured-argument contract.

Source: `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
