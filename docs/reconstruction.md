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
