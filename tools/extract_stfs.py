#!/usr/bin/env python3
import sys
import os
import struct

def extract_stfs(file_path, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    file_size_on_disk = os.path.getsize(file_path)
    print(f"Opening {file_path} (Size: {file_size_on_disk} bytes)...")

    try:
        with open(file_path, 'rb') as f:
            magic = f.read(4)
            print(f"Detected magic: {magic} (hex: {magic.hex()})")

            if magic == b'Rar!':
                print(f"Error: Input file {file_path} is a RAR archive, not an STFS container.")
                print("Please extract it first using '7z x <file>'.")
                return False

            if magic not in [b'LIVE', b'PIRS', b'CON ']:
                print(f"Error: Invalid STFS magic: {magic} (hex: {magic.hex()})")
                return False

            if file_size_on_disk < 0xD000:
                print(f"Error: File is too small to be a valid STFS container ({file_size_on_disk} bytes).")
                return False

            # File table at 0xC000 (Block 0)
            f.seek(0xC000)
            file_table = f.read(0x1000)
            if not file_table or len(file_table) < 0x1000:
                print(f"Error: Could not read file table at 0xC000. Read {len(file_table) if file_table else 0} bytes.")
                return False

            extracted_count = 0
            overall_success = True

            print("Scanning file table...")
            for i in range(0, len(file_table), 0x40):
                entry = file_table[i:i+0x40]
                # If first byte is 0, it's the end of the table
                if entry[0] == 0:
                    break

                try:
                    filename_bytes = entry[0:0x28]
                    filename = filename_bytes.split(b'\0', 1)[0].decode('utf-8', errors='ignore').strip()
                except Exception as e:
                    print(f"Warning: Could not decode filename at entry {i//0x40}: {e}")
                    continue

                if not filename:
                    continue

                # Start Block (0x2F, 3 bytes) - Little Endian (unlike most STFS fields)
                sb_bytes = entry[0x2F:0x32]
                # Size (0x34, 4 bytes) - Big Endian
                size_bytes = entry[0x34:0x38]

                start_block = sb_bytes[0] | (sb_bytes[1] << 8) | (sb_bytes[2] << 16)
                size = struct.unpack('>I', size_bytes)[0]

                print(f"[{extracted_count+1}] Found {filename}: Start Block {start_block}, Size {size} bytes")

                if size == 0:
                    print(f"Skipping empty file {filename}")
                    continue

                if size > 3 * 1024 * 1024 * 1024: # 3GB check
                    print(f"Warning: File {filename} size {size} seems unusually large.")

                # Ensure output subdirectory exists if filename contains paths
                out_path = os.path.join(output_dir, filename)
                out_subdir = os.path.dirname(out_path)
                if out_subdir and not os.path.exists(out_subdir):
                    os.makedirs(out_subdir, exist_ok=True)

                file_success = True
                try:
                    with open(out_path, 'wb') as out_f:
                        remaining = size
                        current_block = start_block

                        while remaining > 0:
                            # Calculate physical offset
                            phys_block = current_block + (current_block // 170) + (current_block // 28900)
                            offset = 0xC000 + phys_block * 0x1000

                            if offset + min(0x1000, remaining) > file_size_on_disk:
                                print(f"Error: Physical offset {offset} for block {current_block} is beyond file size!")
                                file_success = False
                                break

                            f.seek(offset)
                            chunk_size = min(0x1000, remaining)
                            data = f.read(chunk_size)

                            if not data or len(data) < chunk_size:
                                print(f"Error: Unexpected end of file while reading {filename} at block {current_block} (offset {offset})")
                                print(f"Expected {chunk_size} bytes, got {len(data) if data else 0} bytes.")
                                file_success = False
                                break

                            out_f.write(data)
                            remaining -= len(data)
                            current_block += 1
                except IOError as e:
                    print(f"Error: I/O error while writing {filename}: {e}")
                    file_success = False

                if not file_success:
                    print(f"Error: Extraction failed for {filename}. Deleting incomplete file.")
                    try:
                        if os.path.exists(out_path):
                            os.remove(out_path)
                    except OSError:
                        pass
                    overall_success = False
                else:
                    actual_size = os.path.getsize(out_path)
                    if actual_size != size:
                         print(f"Error: Extracted file {filename} size mismatch! Expected {size}, got {actual_size}.")
                         overall_success = False
                    else:
                         print(f"Successfully extracted {filename}")
                         extracted_count += 1

            if extracted_count == 0 and overall_success:
                print("No files found in STFS package file table.")
                return False

            if not overall_success:
                 print("Critical Error: Some files failed to extract or had size mismatches.")
                 return False

        print(f"Extraction complete. {extracted_count} files extracted successfully.")
        return True
    except Exception as e:
        print(f"Critical Exception during extraction: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: extract_stfs.py <input_file> <output_dir>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.exists(input_file):
        print(f"Error: Input file {input_file} does not exist.")
        sys.exit(1)

    success = extract_stfs(input_file, output_dir)
    sys.exit(0 if success else 1)
