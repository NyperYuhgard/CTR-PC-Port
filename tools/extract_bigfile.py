#!/usr/bin/env python3
"""
extract_bigfile.py — Extract CTR BIGFILE.BIG into a folder of loose files.

Usage:
    python3 extract_bigfile.py <path_to_BIGFILE.BIG> [output_dir]

If output_dir is not specified, it defaults to "BIGFILE" (next to the BIGFILE.BIG).

Output format:
    <output_dir>/0.bin
    <output_dir>/1.bin
    <output_dir>/2.bin
    ...
    <output_dir>/N.bin

Each file is named by its BigEntry index (0-based), making it compatible
with the CTR Native dual-read system. To mod a specific asset, simply
replace the corresponding .bin file in the output directory.

The script also generates a mapping file "bigfile_index_map.txt" that
lists each index with its offset and size for reference.
"""

import struct
import sys
import os

BIGENTRY_FORMAT = "<ii"  # little-endian: offset (int), size (int)
BIGENTRY_SIZE = 8
BIGHEADER_SIZE = 8  # cdpos (int) + numEntry (int)


def extract_bigfile(bigfile_path, output_dir):
    """Extract all entries from BIGFILE.BIG into individual .bin files."""

    with open(bigfile_path, "rb") as f:
        # Read header
        header_data = f.read(BIGHEADER_SIZE)
        if len(header_data) < BIGHEADER_SIZE:
            print(f"Error: Could not read BIGFILE header from {bigfile_path}")
            return False

        cdpos, num_entry = struct.unpack("<ii", header_data)
        print(f"BigHeader: cdpos={cdpos}, numEntry={num_entry}")

        if num_entry <= 0 or num_entry > 10000:
            print(f"Error: Unusual numEntry={num_entry}, file may be corrupt")
            return False

        # Read all BigEntry records
        entries_data = f.read(num_entry * BIGENTRY_SIZE)
        if len(entries_data) < num_entry * BIGENTRY_SIZE:
            print(f"Error: Could not read all {num_entry} BigEntry records")
            return False

        entries = []
        for i in range(num_entry):
            offset, size = struct.unpack_from(BIGENTRY_FORMAT, entries_data, i * BIGENTRY_SIZE)
            entries.append((offset, size))

        # Create output directory
        os.makedirs(output_dir, exist_ok=True)

        # Extract each entry
        total_bytes = 0
        extracted = 0

        # Open mapping file
        map_path = os.path.join(output_dir, "bigfile_index_map.txt")
        with open(map_path, "w") as map_file:
            map_file.write("# CTR BigFile Index Map\n")
            map_file.write("# Index | Offset (sectors) | Size (bytes)\n")
            map_file.write("# ------+------------------+-------------\n")

            for i, (offset, size) in enumerate(entries):
                map_file.write(f"{i:5d}  |  {offset:8d}       |  {size:8d}\n")

                if size <= 0:
                    continue

                # Seek to the data position (offset is in sectors from cdpos)
                byte_offset = cdpos + offset * 0x800
                f.seek(byte_offset)

                # Read the data
                data = f.read(size)
                if len(data) < size:
                    print(f"Warning: Could not read full data for entry {i} "
                          f"(got {len(data)}, expected {size})")

                # Write to output file
                out_path = os.path.join(output_dir, f"{i}.bin")
                with open(out_path, "wb") as out_f:
                    out_f.write(data)

                total_bytes += size
                extracted += 1

                if (i + 1) % 50 == 0:
                    print(f"  Extracted {i + 1}/{num_entry} entries...")

        print(f"\nDone! Extracted {extracted} files ({total_bytes:,} bytes total)")
        print(f"Output directory: {output_dir}")
        print(f"Index map: {map_path}")
        print(f"\nTo use unpacked mode:")
        print(f"  1. Place the '{os.path.basename(output_dir)}' folder inside your 'assets/' directory")
        print(f"  2. The game will automatically load from the unpacked folder")
        print(f"  3. To mod a specific asset, replace its <index>.bin file")

    return True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    bigfile_path = sys.argv[1]

    if len(sys.argv) >= 3:
        output_dir = sys.argv[2]
    else:
        # Default: BIGFILE folder next to the BIGFILE.BIG
        base_dir = os.path.dirname(os.path.abspath(bigfile_path))
        output_dir = os.path.join(base_dir, "BIGFILE")

    if not os.path.isfile(bigfile_path):
        print(f"Error: File not found: {bigfile_path}")
        sys.exit(1)

    print(f"Extracting: {bigfile_path}")
    print(f"Output to:  {output_dir}")
    print()

    if not extract_bigfile(bigfile_path, output_dir):
        sys.exit(1)


if __name__ == "__main__":
    main()
