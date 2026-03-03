#!/usr/bin/env python3
import sys
import os
import struct
import concurrent.futures

GDFX_MAGIC = b"MICROSOFT*XBOX*MEDIA"

def extract_file(iso_path, gdfx_offset, sector, size, full_rel_path, output_dir):
    out_full_path = os.path.join(output_dir, full_rel_path)
    os.makedirs(os.path.dirname(out_full_path), exist_ok=True)

    try:
        with open(iso_path, 'rb') as f:
            f.seek(gdfx_offset + (sector * 0x800))
            with open(out_full_path, 'wb') as out_f:
                remaining = size
                while remaining > 0:
                    read_size = min(remaining, 1024 * 1024)
                    chunk = f.read(read_size)
                    if not chunk: break
                    out_f.write(chunk)
                    remaining -= len(chunk)
        return full_rel_path, True, None
    except Exception as e:
        return full_rel_path, False, str(e)

def extract_xgd(iso_path, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    file_size = os.path.getsize(iso_path)
    print(f"Opening ISO: {iso_path} (Size: {file_size} bytes)...")

    try:
        with open(iso_path, 'rb') as f:
            # Read up to 64MB to cover XGD3 partition offsets
            data = f.read(min(file_size, 64 * 1024 * 1024))
            gdfx_offset = -1

            # Check common offsets first for speed
            for off in [0x10000, 0x12000, 0x2080000]:
                if off + 20 <= len(data) and data[off:off+20] == GDFX_MAGIC:
                    gdfx_offset = off
                    break

            if gdfx_offset == -1:
                # Sector-aligned scan fallback
                for offset in range(0, len(data) - 20, 0x800):
                    if data[offset:offset+20] == GDFX_MAGIC:
                        gdfx_offset = offset
                        break

            if gdfx_offset == -1:
                # Last resort: find anywhere in the buffer
                gdfx_offset = data.find(GDFX_MAGIC)

            if gdfx_offset == -1:
                print("Error: Could not find GDFX magic.")
                return False

            print(f"Found GDFX at offset: 0x{gdfx_offset:X}")
            f.seek(gdfx_offset + 0x14)
            root_sector, root_size = struct.unpack('<II', f.read(8))

            queue = [("", root_sector, root_size)]
            files_to_extract = []

            while queue:
                rel_path, dir_sector, dir_size = queue.pop(0)
                f.seek(gdfx_offset + (dir_sector * 0x800))
                dir_data = f.read(dir_size)

                processed_offsets = set()
                pending_node_indices = [0]

                while pending_node_indices:
                    node_index = pending_node_indices.pop(0)
                    offset = node_index * 4
                    if offset in processed_offsets or offset + 14 > len(dir_data): continue
                    processed_offsets.add(offset)

                    l_idx, r_idx, sector, size, attrib, name_len = struct.unpack('<HHIIBB', dir_data[offset:offset+14])
                    if l_idx != 0: pending_node_indices.append(l_idx)
                    if r_idx != 0: pending_node_indices.append(r_idx)
                    if name_len == 0: continue

                    name = dir_data[offset+14 : offset+14+name_len].decode('utf-8', errors='ignore').strip()
                    if not name or name in ['.', '..']: continue

                    full_rel_path = os.path.join(rel_path, name)
                    if attrib & 0x10:
                        if size > 0: queue.append((full_rel_path, sector, size))
                    else:
                        if size > 0: files_to_extract.append((sector, size, full_rel_path))

            print(f"Found {len(files_to_extract)} files. Extracting in parallel...")
            extracted_count = 0
            with concurrent.futures.ThreadPoolExecutor() as executor:
                futures = [executor.submit(extract_file, iso_path, gdfx_offset, *finfo, output_dir) for finfo in files_to_extract]
                for future in concurrent.futures.as_completed(futures):
                    fname, success, error = future.result()
                    if success:
                        print(f"Successfully extracted {fname}")
                        extracted_count += 1
                    else:
                        print(f"Error extracting {fname}: {error}")

            return extracted_count == len(files_to_extract)

    except Exception as e:
        print(f"Critical error: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 3: sys.exit(1)
    success = extract_xgd(sys.argv[1], sys.argv[2])
    sys.exit(0 if success else 1)
