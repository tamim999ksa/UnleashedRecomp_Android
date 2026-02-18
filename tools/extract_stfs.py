#!/usr/bin/env python3
import sys
import os
import struct

def extract_stfs(file_path, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"Opening {file_path}...")
    try:
        with open(file_path, 'rb') as f:
            magic = f.read(4)
            if magic == b'Rar!':
                print(f"Error: Input file {file_path} is a RAR archive, not an STFS container.")
                print("Please extract it first using '7z x <file>'.")
                return False

            if magic not in [b'LIVE', b'PIRS', b'CON ']:
                print(f"Error: Invalid STFS magic: {magic} (hex: {magic.hex()})")
                return False

            # File table at 0xC000 (Block 0)
            f.seek(0xC000)
            file_table = f.read(0x1000)

            extracted_count = 0
            overall_success = True

            for i in range(0, len(file_table), 0x40):
                entry = file_table[i:i+0x40]
                if entry[0] == 0:
                    break

                filename_bytes = entry[0:0x28]
                filename = filename_bytes.split(b'\0', 1)[0].decode('utf-8', errors='ignore')
                if not filename:
                    continue

                # Start Block (0x2F, 3 bytes) - Little Endian (unlike most STFS fields)
                sb_bytes = entry[0x2F:0x32]
                # Size (0x34, 4 bytes) - Big Endian
                size_bytes = entry[0x34:0x38]

                start_block = sb_bytes[0] | (sb_bytes[1] << 8) | (sb_bytes[2] << 16)
                size = struct.unpack('>I', size_bytes)[0]

                print(f"Extracting {filename}: Start Block {start_block}, Size {size}")

                if size > 1024 * 1024 * 1024: # 1GB check
                    print(f"Warning: File {filename} size {size} seems unusually large.")

                out_path = os.path.join(output_dir, filename)
                file_success = True

                with open(out_path, 'wb') as out_f:
                    remaining = size
                    current_block = start_block
                    current_pos = f.tell()

                    while remaining > 0:
                        # Calculate physical offset
                        phys_block = current_block + (current_block // 170) + (current_block // 28900)
                        offset = 0xC000 + phys_block * 0x1000

                        if current_pos != offset:
                            f.seek(offset)
                            current_pos = offset

                        chunk_size = min(0x1000, remaining)
                        data = f.read(chunk_size)
                        if not data or len(data) < chunk_size:
                            print(f"Error: Unexpected end of file while reading {filename} at offset {offset}")
                            print(f"Expected {chunk_size} bytes, got {len(data) if data else 0} bytes.")
                            file_success = False
                            break
                        out_f.write(data)

                        read_len = len(data)
                        remaining -= read_len
                        current_pos += read_len
                        current_block += 1

                if not file_success:
                    print(f"Error: Extraction failed for {filename}. Deleting incomplete file.")
                    try:
                        os.remove(out_path)
                    except OSError:
                        pass
                    overall_success = False
                elif os.path.getsize(out_path) == 0 and size > 0:
                    print(f"Error: Extracted file {filename} is 0 bytes but expected {size}.")
                    try:
                        os.remove(out_path)
                    except OSError:
                        pass
                    overall_success = False

                extracted_count += 1

            if extracted_count == 0:
                print("No files found in STFS package.")
                return False

            if not overall_success:
                 print("Some files failed to extract.")
                 return False

        return True
    except Exception as e:
        print(f"Exception during extraction: {e}")
        return False

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: extract_stfs.py <input_file> <output_dir>")
        sys.exit(1)

    success = extract_stfs(sys.argv[1], sys.argv[2])
    sys.exit(0 if success else 1)
