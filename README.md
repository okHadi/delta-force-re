# Delta Force RE

Reverse engineering Delta Force (1998) for Windows.

## Setup

On an Intel Mac with Node.js 24 and Python 3.10 or later:

```sh
scripts/setup-tools.sh
```

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
