import os
import re

def replace_function(content, func_name, new_body):
    # This is a bit naive but should work for these specific functions
    # We find the function start and match braces
    pattern = rf"(void|bool)\s+{re.escape(func_name)}\s*\([^)]*\)\s*\{{"
    match = re.search(pattern, content)
    if not match:
        print(f"Could not find {func_name}")
        return content

    start = match.start()
    # Find the closing brace
    count = 0
    end = -1
    for i in range(match.end() - 1, len(content)):
        if content[i] == '{':
            count += 1
        elif content[i] == '}':
            count -= 1
            if count == 0:
                end = i + 1
                break

    if end == -1:
        print(f"Could not find closing brace for {func_name}")
        return content

    return content[:start] + new_body + content[end:]

path = 'tools/XenonRecomp/XenonRecomp/recompiler.cpp'
content = open(path).read()

# Fix Recompiler::Analyse
analyse_body = """void Recompiler::Analyse()
{
    functions.reserve(50000);
    for (size_t i = 14; i < 128; i++)
    {
        if (i < 32)
        {
            if (config.restGpr14Address != 0)
            {
                auto& restgpr = functions.emplace_back();
                restgpr.base = config.restGpr14Address + (i - 14) * 4;
                restgpr.size = (32 - i) * 4 + 12;
                image.symbols.emplace(Symbol{ fmt::format("__restgprlr_{}", i), restgpr.base, restgpr.size, Symbol_Function });
            }

            if (config.saveGpr14Address != 0)
            {
                auto& savegpr = functions.emplace_back();
                savegpr.base = config.saveGpr14Address + (i - 14) * 4;
                savegpr.size = (32 - i) * 4 + 8;
                image.symbols.emplace(fmt::format("__savegprlr_{}", i), savegpr.base, savegpr.size, Symbol_Function);
            }

            if (config.restFpr14Address != 0)
            {
                auto& restfpr = functions.emplace_back();
                restfpr.base = config.restFpr14Address + (i - 14) * 4;
                restfpr.size = (32 - i) * 4 + 4;
                image.symbols.emplace(fmt::format("__restfpr_{}", i), restfpr.base, restfpr.size, Symbol_Function);
            }

            if (config.saveFpr14Address != 0)
            {
                auto& savefpr = functions.emplace_back();
                savefpr.base = config.saveFpr14Address + (i - 14) * 4;
                savefpr.size = (32 - i) * 4 + 4;
                image.symbols.emplace(fmt::format("__savefpr_{}", i), savefpr.base, savefpr.size, Symbol_Function);
            }

            if (config.restVmx14Address != 0)
            {
                auto& restvmx = functions.emplace_back();
                restvmx.base = config.restVmx14Address + (i - 14) * 8;
                restvmx.size = (32 - i) * 8 + 4;
                image.symbols.emplace(fmt::format("__restvmx_{}", i), restvmx.base, restvmx.size, Symbol_Function);
            }

            if (config.saveVmx14Address != 0)
            {
                auto& savevmx = functions.emplace_back();
                savevmx.base = config.saveVmx14Address + (i - 14) * 8;
                savevmx.size = (32 - i) * 8 + 4;
                image.symbols.emplace(fmt::format("__savevmx_{}", i), savevmx.base, savevmx.size, Symbol_Function);
            }
        }
    }

    for (auto& section : image.sections)
    {
        if ((section.flags & SectionFlags_Code) == 0)
            continue;

        auto* data = section.data;
        auto* dataEnd = section.data + section.size;
        auto base = section.base;

        while (data < dataEnd)
        {
            auto invalidInstr = config.invalidInstructions.find(ByteSwap(*(uint32_t*)data));
            if (invalidInstr != config.invalidInstructions.end())
            {
                base += invalidInstr->second;
                data += invalidInstr->second;
                continue;
            }

            auto fnSymbol = image.symbols.find(base);
            if (fnSymbol != image.symbols.end() && fnSymbol->type == Symbol_Function)
            {
                size_t skipSize = std::max<size_t>((fnSymbol->address + fnSymbol->size) - base, 4);
                base += skipSize;
                data += skipSize;
            }
            else
            {
                auto& fn = functions.emplace_back(Function::Analyze(data, dataEnd - data, base));
                image.symbols.emplace(fmt::format("sub_{:X}", fn.base), fn.base, fn.size, Symbol_Function);

                size_t advanced = std::max<size_t>(fn.size, 4);
                base += advanced;
                data += advanced;
            }

            if ((base % 0x10000) == 0)
            {
                fmt::println("Scanning section... {:X} / {:X}", base, section.base + section.size);
            }
        }
    }

    std::sort(functions.begin(), functions.end(), [](auto& lhs, auto& rhs) { return lhs.base < rhs.base; });
    fmt::println("Analysis complete. Found {} functions.", functions.size());
}"""

# Fix Recompiler::SaveCurrentOutData
save_body = """void Recompiler::SaveCurrentOutData(const std::string_view& name)
{
    if (!out.empty())
    {
        std::string cppName;
        if (name.empty()) { cppName = fmt.format("ppc_recomp.{}.cpp", cppFileIndex); ++cppFileIndex; }
        bool shouldWrite = true;
        std::string directoryPath = config.directoryPath;
        if (!directoryPath.empty()) directoryPath += "/";
        std::string filePath = fmt::format("{}{}/{}", directoryPath, config.outDirectoryPath, name.empty() ? cppName : name);
        FILE* f = fopen(filePath.c_str(), "rb");
        if (f)
        {
            static std::vector<uint8_t> temp;
            fseek(f, 0, SEEK_END);
            long fileSize = ftell(f);
            if (fileSize == out.size())
            {
                fseek(f, 0, SEEK_SET);
                temp.resize(fileSize);
                if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }
                shouldWrite = !XXH128_isEqual(XXH3_128bits(temp.data(), temp.size()), XXH3_128bits(out.data(), out.size()));
            }
            fclose(f);
        }
        if (shouldWrite)
        {
            f = fopen(filePath.c_str(), "wb");
            fwrite(out.data(), 1, out.size(), f);
            fclose(f);
        }
        std::string().swap(out);
    }
}"""

content = replace_function(content, "Recompiler::Analyse", analyse_body)
content = replace_function(content, "Recompiler::SaveCurrentOutData", save_body)

# Fix Recompiler::Recompile(const Function& fn) to declarations placement
# We find the switch while (base < end) loop and move declarations
# But it's easier to just replace the whole function

recompile_fn_body = """bool Recompiler::Recompile(const Function& fn)
{
    auto base = fn.base;
    auto end = base + fn.size;
    auto* data = (uint32_t*)image.Find(base);

    static std::unordered_set<size_t> labels;
    labels.clear();

    for (size_t addr = base; addr < end; addr += 4)
    {
        const uint32_t instruction = ByteSwap(*(uint32_t*)((char*)data + addr - base));
        if (!PPC_BL(instruction))
        {
            const size_t op = PPC_OP(instruction);
            if (op == PPC_OP_B)
                labels.emplace(addr + PPC_BI(instruction));
            else if (op == PPC_OP_BC)
                labels.emplace(addr + PPC_BD(instruction));
        }

        auto switchTable = config.switchTables.find(addr);
        if (switchTable != config.switchTables.end())
        {
            for (auto label : switchTable->second.labels)
                labels.emplace(label);
        }

        auto midAsmHook = config.midAsmHooks.find(addr);
        if (midAsmHook != config.midAsmHooks.end())
        {
            if (midAsmHook->second.returnOnFalse || midAsmHook->second.returnOnTrue ||
                midAsmHook->second.jumpAddressOnFalse != 0 || midAsmHook->second.jumpAddressOnTrue != 0)
            {
                print("extern bool ");
            }
            else
            {
                print("extern void ");
            }

            print("{}(", midAsmHook->second.name);
            for (auto& reg : midAsmHook->second.registers)
            {
                if (out.back() != '(')
                    out += ", ";

                switch (reg[0])
                {
                case 'c':
                    if (reg == "ctr")
                        print("PPCRegister& ctr");
                    else
                        print("PPCCRRegister& {}", reg);
                    break;
                case 'x':
                    print("PPCXERRegister& xer");
                    break;
                case 'r':
                    print("PPCRegister& {}", reg);
                    break;
                case 'f':
                    if (reg == "fpscr")
                        print("PPCFPSCRRegister& fpscr");
                    else
                        print("PPCRegister& {}", reg);
                    break;
                case 'v':
                    print("PPCVRegister& {}", reg);
                    break;
                }
            }
            println(");\\\\n");

            if (midAsmHook->second.jumpAddress != 0)
                labels.emplace(midAsmHook->second.jumpAddress);
            if (midAsmHook->second.jumpAddressOnTrue != 0)
                labels.emplace(midAsmHook->second.jumpAddressOnTrue);
            if (midAsmHook->second.jumpAddressOnFalse != 0)
                labels.emplace(midAsmHook->second.jumpAddressOnFalse);
        }
    }

    auto symbol = image.symbols.find(fn.base);
    std::string name;
    if (symbol != image.symbols.end()) name = symbol->name;
    else name = fmt::format(\"sub_{:X}\", fn.base);

#ifdef XENON_RECOMP_USE_ALIAS
    println(\"__attribute__((alias(\\\"__imp_{}\\\"))) PPC_WEAK_FUNC({});\", name, name);
#endif

    println(\"PPC_FUNC_IMPL(__imp_{}) {{\", name);
    println(\"\\\\tPPC_FUNC_PROLOGUE();\");

    // Local variable declarations will be placed here AFTER we know which ones are used
    std::string instructions_out;
    std::swap(out, instructions_out);

    auto switchTable = config.switchTables.end();
    bool allRecompiled = true;
    CSRState csrState = CSRState::Unknown;
    RecompilerLocalVariables localVariables;

    ppc_insn insn;
    uint32_t current_base = base;
    uint32_t* current_data = data;
    while (current_base < end)
    {
        if (labels.find(current_base) != labels.end())
        {
            println(\"loc_{:X}:\", current_base);
            csrState = CSRState::Unknown;
        }
        if (switchTable == config.switchTables.end())
            switchTable = config.switchTables.find(current_base);

        ppc::Disassemble(current_data, 4, current_base, insn);
        if (insn.opcode == nullptr)
        {
            println(\"\\\\t// {}\", insn.op_str);
        }
        else
        {
            if (!Recompile({ fn, current_base, insn, current_data, switchTable, localVariables, csrState }))
            {
                fmt::println(\"Unrecognized instruction at 0x{:X}: {}\", current_base, insn.opcode->name);
                allRecompiled = false;
            }
        }
        current_base += 4;
        ++current_data;
    }

    std::string body_out;
    std::swap(out, body_out);

    // Restore instructions_out (prologue)
    out = instructions_out;

    if (localVariables.ctr) println(\"\\\\tPPCRegister ctr{{}};\");
    if (localVariables.xer) println(\"\\\\tPPCXERRegister xer{{}};\");
    if (localVariables.reserved) println(\"\\\\tPPCRegister reserved{{}};\");
    for (size_t i = 0; i < 8; i++) if (localVariables.cr[i]) println(\"\\\\tPPCCRRegister cr{}{{{{}}}};\", i);
    for (size_t i = 0; i < 32; i++) if (localVariables.r[i]) println(\"\\\\tPPCRegister r{}{{{{}}}};\", i);
    for (size_t i = 0; i < 32; i++) if (localVariables.f[i]) println(\"\\\\tPPCRegister f{}{{{{}}}};\", i);
    for (size_t i = 0; i < 128; i++) if (localVariables.v[i]) println(\"\\\\tPPCVRegister v{}{{{{}}}};\", i);
    if (localVariables.env) println(\"\\\\tPPCContext env{{}};\");
    if (localVariables.temp) println(\"\\\\tPPCRegister temp{{}};\");
    if (localVariables.vTemp) println(\"\\\\tPPCVRegister vTemp{{}};\");
    if (localVariables.ea) println(\"\\\\tuint32_t ea{{}};\");

    out += body_out;
    println(\"}}\\\\n\");

#ifndef XENON_RECOMP_USE_ALIAS
    println(\"PPC_WEAK_FUNC({}) {{\", name);
    println(\"\\\\t__imp_{}(ctx, base);\", name);
    println(\"}}\\\\n\");
#endif

    return allRecompiled;
}"""

# Fix escaped backslashes for python
recompile_fn_body = recompile_fn_body.replace('\\\\', '\\')

content = replace_function(content, "Recompiler::Recompile", recompile_fn_body)

open(path, 'w').write(content)
"

# 5. Patch function.cpp surgically
cat <<'EOF' > patch_function.py
import os
import re

path = 'tools/XenonRecomp/XenonAnalyse/function.cpp'
content = open(path).read()

# Fix Function::Analyze
# Add minimum size guarantee at the end and the tail call check at the start
analyse_pattern = r"Function Function::Analyze\(const void\* code, size_t size, size_t base\)\s+\{"
new_start = """Function Function::Analyze(const void* code, size_t size, size_t base)
{
    Function fn{ base, 0 };
    const auto* dataStart = (const uint32_t*)code;

    if (size >= 8 && dataStart[1] == 0x48000004) // ByteSwap(0x04000048)
    {
        fn.size = 0x8;
        return fn;
    }"""

content = re.sub(analyse_pattern, new_start, content)

# Add min size guarantee at end
end_pattern = r"return fn;\s+\}"
new_end = """    // Ensure we always return at least 4 bytes if any data was processed to prevent infinite loops
    if (fn.size == 0 && data > dataStart) fn.size = 4;
    return fn;
}"""

content = re.sub(end_pattern, new_end, content)
open(path, 'w').write(content)
