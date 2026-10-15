# Delta Force RE

Reverse engineering Delta Force (1998) for Windows.

## Setup

On an Intel Mac with Node.js 24 and Python 3.10 or later:

```sh
scripts/setup-tools.sh
```

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For Windows builds on this Mac, use [Windows build](docs/windows-build.md).

## Run

```sh
build/Release/dfre.exe "C:/Games/Delta Force"
```

Download the Windows executable from the latest successful [Tests run](https://github.com/okHadi/delta-force-re/actions/workflows/tests.yml). Keep the original game files on your device.

The current build has soldier profiles, campaign selection, mission briefing, and the initial mission scene with terrain, water, and world models. Movement, combat, and multiplayer are in development. Saves use `%LOCALAPPDATA%/DeltaForceRE`.

## Analysis

```sh
python3 tools/pff.py list "/path/to/Delta Force/Df.pff"
python3 tools/pff.py extract "/path/to/Delta Force/Df.pff" DFG3_D.PCX .local/terrain.pcx
python3 tools/pcx.py .local/terrain.pcx
.local/tools/python/bin/python tools/pe_report.py "/path/to/Delta Force/Df.exe"
scripts/rea analyze "/path/to/Delta Force/Df.exe" --provider ghidra --json
```

Keep game files and generated reports in `local/` or `.local/`.

## Tests

```sh
.local/tools/python/bin/python -m unittest discover -s tests
```
