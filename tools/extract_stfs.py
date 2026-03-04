#!/usr/bin/env python3
import sys
import os
import struct
import concurrent.futures

def extract_file(file_path, output_dir, filename, start_block, size, file_size_on_disk):
    out_path = os.path.join(output_dir, filename)
    out_subdir = os.path.dirname(out_path)
    if out_subdir and not os.path.exists(out_subdir):
        os.makedirs(out_subdir, exist_ok=True)

    try:
        with open(file_path, 'rb') as f:
            with open(out_path, 'wb') as out_f:
                remaining = size
                current_block = start_block

                while remaining > 0:
                    phys_block = current_block + (current_block // 170) + (current_block // 28900)
                    offset = 0xC000 + phys_block * 0x1000

                    if offset + min(0x1000, remaining) > file_size_on_disk:
                        return filename, False, "Beyond file size"

                    f.seek(offset)
                    chunk_size = min(0x1000, remaining)
                    data = f.read(chunk_size)

                    if not data or len(data) < chunk_size:
                        return filename, False, f"Unexpected EOF at block {current_block}"

                    out_f.write(data)
                    remaining -= len(data)
                    current_block += 1

        if os.path.getsize(out_path) != size:
            return filename, False, "Size mismatch"

        return filename, True, None
    except Exception as e:
        return filename, False, str(e)

def extract_stfs(file_path, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    file_size_on_disk = os.path.getsize(file_path)
    print(f"Opening STFS: {file_path} (Size: {file_size_on_disk} bytes)...")

    try:
        with open(file_path, 'rb') as f:
            magic = f.read(4)
            if magic not in [b'LIVE', b'PIRS', b'CON ']:
                print(f"Error: Invalid STFS magic: {magic}")
                return False

            f.seek(0xC000)
            file_table = f.read(0x1000)
            if not file_table or len(file_table) < 0x1000:
                print("Error: Could not read file table.")
                return False

            entries = []
            for i in range(0, len(file_table), 0x40):
                entry = file_table[i:i+0x40]
                if entry[0] == 0: break

                name_bytes = entry[0:0x28]
                filename = name_bytes.split(b'\0', 1)[0].decode('utf-8', errors='ignore').strip()
                if not filename: continue

                sb_bytes = entry[0x2F:0x32]
                size_bytes = entry[0x34:0x38]
                start_block = sb_bytes[0] | (sb_bytes[1] << 8) | (sb_bytes[2] << 16)
                size = struct.unpack('>I', size_bytes)[0]

                if size > 0:
                    entries.append((filename, start_block, size))

            if not entries:
                print("No files found in STFS.")
                return False

            print(f"Found {len(entries)} files. Extracting in parallel...")
            extracted_count = 0
            with concurrent.futures.ThreadPoolExecutor() as executor:
                futures = [executor.submit(extract_file, file_path, output_dir, *entry, file_size_on_disk) for entry in entries]
                for future in concurrent.futures.as_completed(futures):
                    filename, success, error = future.result()
                    if success:
                        print(f"Successfully extracted {filename}")
                        extracted_count += 1
                    else:
                        print(f"Error extracting {filename}: {error}")
                        # Don't fail immediately, try others

            return extracted_count == len(entries)

    except Exception as e:
        print(f"Critical error: {e}")
        return False

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(1)
    success = extract_stfs(sys.argv[1], sys.argv[2])
    sys.exit(0 if success else 1)
