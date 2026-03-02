#!/usr/bin/env python3
import sys
import os
import struct

# GDFX (Xbox Media) filesystem extractor
# Based on documentation and common Xbox 360 ISO structures

GDFX_MAGIC = b"MICROSOFT*XBOX*MEDIA"

def extract_xgd(iso_path, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    file_size = os.path.getsize(iso_path)
    print(f"Opening ISO: {iso_path} (Size: {file_size} bytes)...")

    try:
        with open(iso_path, 'rb') as f:
            # Look for GDFX magic at common offsets (XGD2: 0x10000, XGD3: 0x12000)
            # We'll scan the first 1MB just in case
            gdfx_offset = -1
            scan_size = min(file_size, 1024 * 1024)
            data = f.read(scan_size)

            # Sectors are usually 0x800 (2048 bytes) aligned.
            for offset in range(0, scan_size - 20, 0x800):
                if data[offset:offset+20] == GDFX_MAGIC:
                    gdfx_offset = offset
                    break

            if gdfx_offset == -1:
                # Fallback: scan every byte if sector alignment didn't work
                gdfx_offset = data.find(GDFX_MAGIC)

            if gdfx_offset == -1:
                print("Error: Could not find GDFX magic 'MICROSOFT*XBOX*MEDIA' in ISO.")
                return False

            print(f"Found GDFX filesystem at offset: 0x{gdfx_offset:X}")

            # GDFX Header:
            # 0x00: Magic (20 bytes)
            # 0x14: Root Directory Sector (4 bytes)
            # 0x18: Root Directory Size (4 bytes)
            # 0x1C: Creation Time (8 bytes)
            f.seek(gdfx_offset + 0x14)
            root_sector_data = f.read(8)
            if len(root_sector_data) < 8:
                 print("Error: Could not read root directory metadata from GDFX header.")
                 return False

            root_sector, root_size = struct.unpack('<II', root_sector_data)
            print(f"Root Directory: Sector {root_sector}, Size {root_size} bytes")

            # Recursive traversal of the GDFX directory tree
            queue = [("", root_sector, root_size)]
            extracted_count = 0

            while queue:
                rel_path, dir_sector, dir_size = queue.pop(0)

                # Read the directory sector(s)
                f.seek(gdfx_offset + (dir_sector * 0x800))
                dir_data = f.read(dir_size)

                # Each directory is a binary search tree. Each node is 14 bytes + name.
                # Format:
                # 0x00: Left Child index (uint16)
                # 0x02: Right Child index (uint16)
                # 0x04: Data Sector (uint32)
                # 0x08: Data Size (uint32)
                # 0x0C: Attributes (uint8)
                # 0x0D: Name Length (uint8)
                # 0x0E: Name (char[name_len])

                # The indices are node indices within the same directory entry list.
                # Nodes are 4-byte aligned. Each node index is (offset / 4).

                # To simplify and ensure we don't miss anything, we'll scan the whole directory buffer.
                # Valid nodes must have a non-zero name length and valid child pointers.

                # Use a set to track processed offsets to avoid infinite loops if BST is corrupted
                processed_offsets = set()
                pending_node_indices = [0] # Root of the directory tree is always node 0

                while pending_node_indices:
                    node_index = pending_node_indices.pop(0)
                    offset = node_index * 4

                    if offset in processed_offsets or offset + 14 > len(dir_data):
                        continue
                    processed_offsets.add(offset)

                    l_idx, r_idx, sector, size, attrib, name_len = struct.unpack('<HHIIBB', dir_data[offset:offset+14])

                    # Add children to pending list
                    if l_idx != 0:
                        pending_node_indices.append(l_idx)
                    if r_idx != 0:
                        pending_node_indices.append(r_idx)

                    if name_len == 0:
                        continue

                    name_bytes = dir_data[offset+14 : offset+14+name_len]
                    try:
                        name = name_bytes.decode('utf-8', errors='ignore').strip()
                    except:
                        continue

                    if not name or name in ['.', '..']:
                        continue

                    full_rel_path = os.path.join(rel_path, name)

                    if attrib & 0x10: # Directory attribute (0x10)
                        if size > 0:
                            queue.append((full_rel_path, sector, size))
                    else: # File
                        print(f"Found {full_rel_path} ({size} bytes)...")
                        # We only care about mandatory files, but for robustness we'll extract them.
                        # Flatten to root of output_dir OR keep structure?
                        # release.yml's finalize_file handles flat/lowercase but structure is safer.

                        out_full_path = os.path.join(output_dir, full_rel_path)
                        os.makedirs(os.path.dirname(out_full_path), exist_ok=True)

                        try:
                            f.seek(gdfx_offset + (sector * 0x800))
                            with open(out_full_path, 'wb') as out_f:
                                remaining = size
                                while remaining > 0:
                                    read_size = min(remaining, 1024 * 1024)
                                    chunk = f.read(read_size)
                                    if not chunk:
                                        print(f"Warning: Unexpected EOF reading {full_rel_path}")
                                        break
                                    out_f.write(chunk)
                                    remaining -= len(chunk)
                            extracted_count += 1
                        except Exception as e:
                            print(f"Error extracting {full_rel_path}: {e}")

            print(f"Extraction complete. Extracted {extracted_count} files.")
            return True

    except Exception as e:
        print(f"Critical error: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: extract_xgd.py <iso_path> <output_dir>")
        sys.exit(1)

    iso_path = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.exists(iso_path):
        print(f"Error: {iso_path} does not exist.")
        sys.exit(1)

    success = extract_xgd(iso_path, output_dir)
    sys.exit(0 if success else 1)
