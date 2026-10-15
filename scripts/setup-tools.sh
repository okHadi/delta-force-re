#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
[ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = x86_64 ] || {
  echo 'Run this setup on an Intel Mac.' >&2
  exit 1
}
for command in curl shasum unzip tar node npm python3 make g++ bison xcode-select; do
  command -v "$command" >/dev/null
done
xcode-select -p >/dev/null
node -e 'const [m,n]=process.versions.node.split(".").map(Number); if (!((m===22&&n>=19)||(m===24&&n>=11)||m>=26)) { console.error("Use Node.js 22.19+, 24.11+, or 26+."); process.exit(1); }'
python3 -c 'import sys; sys.exit("Use Python 3.10 or later.") if sys.version_info < (3,10) else None'
mkdir -p .local/tools .local/reports
fetch() {
  file=$1
  url=$2
  expected=$3
  if [ ! -f "$file" ] || [ "$(shasum -a 256 "$file" | cut -d ' ' -f 1)" != "$expected" ]; then
    if [ -e "$file" ]; then
      invalid=$(mktemp "$file.invalid.XXXXXX")
      mv "$file" "$invalid"
    fi
    curl -fL --retry 2 "$url" -o "$file"
  fi
  printf '%s  %s\n' "$expected" "$file" | shasum -a 256 -c -
}
fetch .local/tools/java.tar.gz \
  'https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jdk_x64_mac_hotspot_21.0.12.1_1.tar.gz' \
  44db0f08196daf19a47f90d13388b0c943b67663cb537f998fe29e836fa842ce
if [ ! -x .local/tools/jdk-21.0.12.1+1/Contents/Home/bin/java ]; then
  tar -xzf .local/tools/java.tar.gz -C .local/tools
fi
fetch .local/tools/ghidra.zip \
  'https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20260921.zip' \
  ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db
if [ ! -d .local/tools/ghidra_12.1.4_PUBLIC ]; then
  unzip -q .local/tools/ghidra.zip -d .local/tools
fi
ghidra="$root/.local/tools/ghidra_12.1.4_PUBLIC"
if [ ! -x "$ghidra/Ghidra/Features/Decompiler/os/mac_x86_64/decompile" ]; then
  (
    cd "$ghidra/Ghidra/Features/Decompiler/src/decompile/cpp"
    make -j2 ghidra_opt
    mkdir -p "$ghidra/Ghidra/Features/Decompiler/os/mac_x86_64"
    install -m 755 ghidra_opt "$ghidra/Ghidra/Features/Decompiler/os/mac_x86_64/decompile"
  )
fi
if [ ! -x .local/tools/rea/node_modules/.bin/rea ] || \
   ! cmp -s tools/rea/package-lock.json .local/tools/rea/package-lock.json; then
  mkdir -p .local/tools/rea
  cp tools/rea/package.json tools/rea/package-lock.json .local/tools/rea/
  npm ci --prefix .local/tools/rea --no-audit --no-fund
fi
if [ ! -x .local/tools/python/bin/python ]; then
  python3 -m venv .local/tools/python
fi
.local/tools/python/bin/python -m pip install -r requirements.txt
scripts/rea doctor --provider ghidra --json
