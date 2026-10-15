# Delta Force RE

Reverse engineering Delta Force (1998) for Windows.

## Archives

```sh
python3 tools/pff.py list "/path/to/Delta Force/Df.pff"
python3 tools/pff.py extract "/path/to/Delta Force/Df.pff" M16.3DI .local/M16.3DI
```

Keep game files and generated reports in `local/` or `.local/`.

## Tests

```sh
python3 -m unittest discover -s tests
```
