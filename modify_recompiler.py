import re

file_path = 'tools/XenonRecomp/XenonRecomp/recompiler.cpp'

with open(file_path, 'r') as f:
    content = f.read()

# Replace definition
definition_pattern = r'bool Recompiler::Recompile\(\s*const Function& fn,\s*uint32_t base,\s*const ppc_insn& insn,\s*const uint32_t\* data,\s*std::unordered_map<uint32_t, RecompilerSwitchTable>::iterator& switchTable,\s*RecompilerLocalVariables& localVariables,\s*CSRState& csrState\)\s*\{'

replacement_definition = """bool Recompiler::Recompile(const RecompileArgs& args)
{
    const auto& fn = args.fn;
    auto base = args.base;
    const auto& insn = args.insn;
    const auto* data = args.data;
    auto& switchTable = args.switchTable;
    auto& localVariables = args.localVariables;
    auto& csrState = args.csrState;"""

if re.search(definition_pattern, content):
    content = re.sub(definition_pattern, replacement_definition, content)
else:
    print("Could not find definition pattern")
    exit(1)

# Replace call site
call_site_pattern = r'if \(!Recompile\(fn, base, insn, data, switchTable, localVariables, csrState\)\)'
replacement_call_site = 'if (!Recompile({ fn, base, insn, data, switchTable, localVariables, csrState }))'

if re.search(call_site_pattern, content):
    content = re.sub(call_site_pattern, replacement_call_site, content)
else:
    print("Could not find call site pattern")
    # try looking for it with inconsistent whitespace just in case
    # actually the grep showed it on one line but let's be safe
    pass

with open(file_path, 'w') as f:
    f.write(content)

print("Successfully modified recompiler.cpp")
