#!/usr/bin/env python3
"""Read a PE file and print a static inventory as JSON."""

import argparse
import hashlib
import json
from pathlib import Path
import re

import pefile


def decode(value):
    return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else value


def ascii_strings(data, minimum=4):
    """Yield printable ASCII runs and their exact file offsets."""
    for match in re.finditer(rb"[\x20-\x7e]{" + str(minimum).encode() + rb",}", data):
        yield match.start(), match.group().decode("ascii")


def mapped_rva(pe, offset):
    """Return an RVA only for file bytes mapped by PE headers or a section."""
    if 0 <= offset < min(pe.OPTIONAL_HEADER.SizeOfHeaders, len(pe.__data__)):
        return offset
    for section in pe.sections:
        start = section.PointerToRawData
        if start == 0 or section.Characteristics & 0x80:
            continue
        if start <= offset < start + section.SizeOfRawData:
            return section.VirtualAddress + offset - start
    return None


def inventory(path, string_pattern=None, minimum=4, limit=200):
    data = path.read_bytes()
    pe = pefile.PE(data=data)
    base = pe.OPTIONAL_HEADER.ImageBase
    sections = []
    for section in pe.sections:
        raw_start = section.PointerToRawData
        raw_end = raw_start + section.SizeOfRawData
        uninitialized = bool(section.Characteristics & 0x80)
        sections.append({
            "name": decode(section.Name.rstrip(b"\0")),
            "rva_start": section.VirtualAddress,
            "rva_end": section.VirtualAddress + max(section.Misc_VirtualSize, section.SizeOfRawData),
            "virtual_size": section.Misc_VirtualSize,
            "raw_start": raw_start,
            "raw_end": raw_end,
            "raw_size": section.SizeOfRawData,
            "raw_range_in_file": raw_end <= len(data),
            "uninitialized_data": uninitialized,
            "characteristics": section.Characteristics,
            "entropy": None if uninitialized else round(section.get_entropy(), 3),
        })
    imports = {}
    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        imports[decode(entry.dll)] = [
            {"name": decode(item.name), "ordinal": item.ordinal,
             "iat_va": item.address} for item in entry.imports
        ]
    exports = []
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        exports = [{"name": decode(item.name), "ordinal": item.ordinal,
                    "rva": item.address, "forwarder": decode(item.forwarder)}
                   for item in pe.DIRECTORY_ENTRY_EXPORT.symbols]
    versions = []
    for group in getattr(pe, "FileInfo", []):
        for info in group:
            for table in getattr(info, "StringTable", []):
                versions.append({decode(k): decode(v) for k, v in table.entries.items()})
    matches = []
    match_count = 0
    if string_pattern is not None:
        pattern = re.compile(string_pattern, re.IGNORECASE)
        for offset, value in ascii_strings(data, minimum):
            if pattern.search(value):
                match_count += 1
                if len(matches) < limit:
                    rva = mapped_rva(pe, offset)
                    matches.append({"offset": offset, "rva": rva,
                                    "va": None if rva is None else base + rva,
                                    "text": value})
    result = {
        "path": str(path.resolve()), "file_size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "machine": pefile.MACHINE_TYPE.get(pe.FILE_HEADER.Machine, pe.FILE_HEADER.Machine),
        "optional_header_magic": pe.OPTIONAL_HEADER.Magic,
        "image_base": base, "image_size": pe.OPTIONAL_HEADER.SizeOfImage,
        "entry_rva": pe.OPTIONAL_HEADER.AddressOfEntryPoint,
        "entry_va": base + pe.OPTIONAL_HEADER.AddressOfEntryPoint,
        "linker_version": f"{pe.OPTIONAL_HEADER.MajorLinkerVersion}.{pe.OPTIONAL_HEADER.MinorLinkerVersion}",
        "sections": sections, "imports": imports, "exports": exports,
        "version_fields": versions, "pe_warnings": pe.get_warnings(),
        "strings": matches, "string_match_count": match_count,
        "strings_truncated": match_count > limit,
    }
    pe.close()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("--strings", help="Case-insensitive regex; omit to skip strings")
    parser.add_argument("--min-string", type=int, default=4)
    parser.add_argument("--max-strings", type=int, default=200)
    args = parser.parse_args()
    if args.min_string < 1 or args.max_strings < 0:
        parser.error("String minimum must be positive; maximum must be zero or greater")
    try:
        report = inventory(args.file, args.strings, args.min_string, args.max_strings)
    except (OSError, pefile.PEFormatError, re.error) as exc:
        parser.exit(1, f"pe_report: {exc}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
