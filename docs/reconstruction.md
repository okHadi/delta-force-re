# Reconstructed code

Target: `Df.exe`, SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

## Filename extension helper

- Original: `0x456560`–`0x4565ef`, 144 bytes.
- REA evidence: `ev_2eb2e334c91527077107c5f785447ebfaddf9485c1412e50ed46d9fe9c51c532`.
- Code: `src/filename.cpp`.
- Replace text after the first dot. Add a dot when absent. Copy extension text as supplied.
- Path separators have no special meaning. Inputs contain no NUL bytes.
- 2,072 cases match the original x86 instructions under Unicorn 2.1.4.
- The check covers output bytes, using an owned C++ result. Original buffer aliasing and undefined behavior are outside its scope.

```sh
.local/tools/python/bin/python -m pip install -r requirements-re.txt
.local/tools/python/bin/python tools/check_filename.py "/path/to/Delta Force/Df.exe" build/filename_probe
```

On a Windows multi-configuration build, use `build/Release/filename_probe.exe`.

## State loop

- Original dispatcher: `0x461270`–`0x46133e`.
- Code: `src/state_loop.cpp`.
- State callbacks: enter at `+0x20`, update at `+0x24`, render at `+0x28`, leave at `+0x2c`.
- Run updates in 16 ms steps. Start with one step ready. Carry pending time across state changes.
- Render after completed updates. An update return value other than zero leaves the state.
- Read the next state after the leave callback. A null next state returns from the loop.
- Use unsigned 32-bit clock differences, including clock wrap.
- 204 cases match original x86 callback order, clock reads, yield calls, and elapsed totals under Unicorn 2.1.4.

```sh
.local/tools/python/bin/python tools/check_state_loop.py "/path/to/Delta Force/Df.exe" build/state_loop_probe
```

Full local evidence: `startup-menu-start.json`, `state-loop-differential.json` in `.local/reports/`.
