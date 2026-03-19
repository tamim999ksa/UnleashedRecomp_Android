with open('tools/XenosRecomp/XenosRecomp/main.cpp', 'rb') as f:
    content = f.read()

# Cleaning up and using a simple version
content = content.replace(b'f.print("const uint8_t g_compressedDxilCache[] = {\\x5c\\x6e\\x5c\\x74\\x5c\\x22");', b'f.print("const uint8_t g_compressedDxilCache[] = {\\"");')
content = content.replace(b'f.print("\\"\\x5c\\x6e\\x5c\\x74\\x5c\\x22");', b'f.print("\\"\\"");')
content = content.replace(b'f.print("const uint8_t g_compressedSpirvCache[] = {\\"");', b'f.print("const uint8_t g_compressedSpirvCache[] = {\\"");')

with open('tools/XenosRecomp/XenosRecomp/main.cpp', 'wb') as f:
    f.write(content)
