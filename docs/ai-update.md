# Actor ticks

`tick_ai` runs the original actor schedule over groups 3, 0, the extra pool, 1, and 2. Pass `AiTickHooks` to run native calls, callbacks, and profiler marks at their original points. The returned report records those calls.

Callbacks use current actor fields. Group 3 countdowns run after the callback. Group 0 reads its secondary callback after its primary callback. Keep actor slots stable; hooks can replace vector storage or add slots.

`rebuild_target_lists` refreshes nearby-actor lists after 64 ticks. Bounds come from the loaded model. Pause and demo flags select the original branches.

```sh
python3 tools/check_ai_update_original.py GAME_FOLDER AI_UPDATE_PROBE --mutating-callbacks --report .local/ai-hooks.json
```

The check compares the original schedule, callback order, countdowns, and full actor records with scripted callback writes. The callbacks in this check are test providers. Native NPC behavior has separate checks.

Source: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. Entries: `487EA0`, `487E10`, `487C50`.
