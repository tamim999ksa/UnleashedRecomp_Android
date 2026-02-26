import sys

filepath = "UnleashedRecomp/kernel/imports.cpp"

with open(filepath, "r") as f:
    content = f.read()

replacements = [
    (
        'void VdHSIOCalibrationLock()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid VdHSIOCalibrationLock()\n{\n    LOG_UTILITY("STUB: VdHSIOCalibrationLock - Variable access not implemented");\n}'
    ),
    (
        'void KeCertMonitorData()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid KeCertMonitorData()\n{\n    LOG_UTILITY("STUB: KeCertMonitorData - Variable access not implemented");\n}'
    ),
    (
        'void XexExecutableModuleHandle()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid XexExecutableModuleHandle()\n{\n    LOG_UTILITY("STUB: XexExecutableModuleHandle - Variable access not implemented");\n}'
    ),
    (
        'void ExLoadedCommandLine()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid ExLoadedCommandLine()\n{\n    LOG_UTILITY("STUB: ExLoadedCommandLine - Variable access not implemented");\n}'
    ),
    (
        'void KeDebugMonitorData()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid KeDebugMonitorData()\n{\n    LOG_UTILITY("STUB: KeDebugMonitorData - Variable access not implemented");\n}'
    ),
    (
        'void ExThreadObjectType()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid ExThreadObjectType()\n{\n    LOG_UTILITY("STUB: ExThreadObjectType - Variable access not implemented");\n}'
    ),
    (
        'void KeTimeStampBundle()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid KeTimeStampBundle()\n{\n    LOG_UTILITY("STUB: KeTimeStampBundle - Variable access not implemented");\n}'
    ),
    (
        'void XboxHardwareInfo()\n{\n    LOG_UTILITY("!!! STUB !!!");\n}',
        '// Note: Exported as kVariable in xboxkrnl_table.inc\nvoid XboxHardwareInfo()\n{\n    LOG_UTILITY("STUB: XboxHardwareInfo - Variable access not implemented");\n}'
    ),
]

for old, new in replacements:
    content = content.replace(old, new)

with open(filepath, "w") as f:
    f.write(content)
