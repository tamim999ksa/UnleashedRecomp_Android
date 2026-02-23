import sys

with open('UnleashedRecomp/CMakeLists.txt', 'r') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if '"ui/installer_wizard.cpp"' in line:
        continue # Remove from list

    if 'set(UNLEASHED_RECOMP_UI_CXX_SOURCES' in line:
        new_lines.append(line)
        continue

    if '"ui/tv_static.cpp"' in line:
        new_lines.append(line)
        # Verify next line is )
        continue

    if line.strip() == ')' and len(new_lines) > 0 and 'tv_static.cpp' in new_lines[-1]:
        new_lines.append(line)
        new_lines.append('\n')
        new_lines.append('if (NOT ANDROID)\n')
        new_lines.append('    list(APPEND UNLEASHED_RECOMP_UI_CXX_SOURCES "ui/installer_wizard.cpp")\n')
        new_lines.append('endif()\n')
        continue

    if '## Installer ##' in line:
        new_lines.append('if (NOT ANDROID)\n')
        new_lines.append(line)
        continue

    if 'BIN2C(TARGET_OBJ UnleashedRecomp SOURCE_FILE "/images/installer/pulse_install.dds"' in line:
        new_lines.append(line)
        new_lines.append('endif()\n')
        continue

    if 'BIN2C(TARGET_OBJ UnleashedRecomp SOURCE_FILE "/music/installer.ogg"' in line:
        new_lines.append('if (NOT ANDROID)\n')
        new_lines.append(line)
        new_lines.append('endif()\n')
        continue

    new_lines.append(line)

with open('UnleashedRecomp/CMakeLists.txt', 'w') as f:
    f.writelines(new_lines)
