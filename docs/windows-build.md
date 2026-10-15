# Windows build on Intel Mac

Install local tools:

```sh
scripts/setup-runtime-tools.sh
```

Build Windows x64 code:

```sh
cmake -S . -B .local/build/windows -DCMAKE_TOOLCHAIN_FILE=cmake/windows-zig.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .local/build/windows -j 4
ctest --test-dir .local/build/windows --output-on-failure
```

Run Windows programs through `scripts/wine`. CTest uses a separate `.local/wine-tests/` prefix. Interactive game runs use `.local/wine-prefix/`.

## Game build

```sh
scripts/wine .local/build/windows/dfre.exe 'Z:\path\to\Delta Force'
```

Use `--save-dir PATH` to set a separate save folder. Use `--frames N` for a short display check.

## Original reference

Run a copy of the supplied game from `.local/reference-game/`. Keep the source files unchanged. Use `scripts/wine` with `Df.exe` from the copied folder.

`reference_capture OUTPUT.ppm` reads the original process's 32-bit display buffer. It checks the window version and dispatcher bytes. Use a new output path. The tool reads process memory and writes an image; it keeps game memory unchanged.

Target executable SHA-256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

## Window backend

Windows uses Win32 input and a 32-bit DIB display. Mac development builds use SDL2 and a local ad hoc code signature:

```sh
cmake -S . -B build -DDFRE_BUILD_WINDOW=ON
cmake --build build -j 4
build/window_probe "/path/to/Delta Force" DFSPLASH.PCX
```

`window_probe` checks image display. Game states use the same window interface.
