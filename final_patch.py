import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return
    with open(path, 'r') as f:
        content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(path, 'w') as f:
        f.write(new_content)

# 1. symbol.h - make size mutable
patch_file('tools/XenonRecomp/XenonUtils/symbol.h',
           r'size_t size\{\};',
           'mutable size_t size{};')

# 2. symbol_table.h - range lookup
symbol_table_find = """    const_iterator find(size_t address) const
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        for (int i = 0; i < 32; ++i) {
            if (address >= curr->address && address < curr->address + curr->size) return curr;
            if (address == curr->address) return curr;
            if (curr == begin()) break;
            curr = std::prev(curr);
            if (address > curr->address + 1024 * 1024) break;
        }
        return end();
    }

    iterator find(size_t address)
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        for (int i = 0; i < 32; ++i) {
            if (address >= curr->address && address < curr->address + curr->size) return curr;
            if (address == curr->address) return curr;
            if (curr == begin()) break;
            curr = std::prev(curr);
            if (address > curr->address + 1024 * 1024) break;
        }
        return end();
    }"""

patch_file('tools/XenonRecomp/XenonUtils/symbol_table.h',
           r'const_iterator find\(size_t address\) const.*?iterator find\(size_t address\).*?\}',
           symbol_table_find)

# 3. function.cpp - tail call check and min size guarantee
# Scoping fix: put tail call check AFTER dataStart/dataEnd
fn_analyze_start = """Function Function::Analyze(const void* code, size_t size, size_t base)
{
    Function fn{ base, 0 };

    const auto* dataStart = (const uint32_t*)code;
    const auto* dataEnd = (const uint32_t*)((const uint8_t*)code + size);

    if (size >= 8 && dataStart[1] == 0x48000004) // ByteSwap(0x04000048)
    {
        fn.size = 0x8;
        return fn;
    }

    auto& blocks = fn.blocks;"""

patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'Function Function::Analyze\(const void\* code, size_t size, size_t base\).*?auto& blocks = fn.blocks;',
           fn_analyze_start)

patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'return fn;\s+\}',
           '    if (fn.size == 0 && data > dataStart) fn.size = 4;\n    return fn;\n}')

# 4. recompiler.cpp - Analyse fixes, SaveCurrentOutData, Recompile
# Update reservation
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'functions\.reserve\(8192\);',
           'functions.reserve(50000);')

# Analyse logic - pre-analyze pre-defined symbols and skip identified ranges
analyse_logic = """    for (auto& section : image.sections)
    {
        if ((section.flags & SectionFlags_Code) == 0)
            continue;

        auto* data = section.data;
        auto* dataEnd = section.data + section.size;
        auto base = section.base;

        // Pre-analyze pre-defined symbols to establish baseline ranges
        for (auto it = image.symbols.lower_bound(section.base); it != image.symbols.end() && it->address < section.base + section.size; ++it)
        {
            if (it->type == Symbol_Function)
            {
                auto data_ptr = section.data + it->address - section.base;
                auto& fn = functions.emplace_back(Function::Analyze(data_ptr, section.base + section.size - it->address, it->address));
                it->size = fn.size; // Update the symbol size!
            }
        }

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
    }"""

patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'for \(auto& section : image\.sections\).*?fmt::println\("Analysis complete\. Found \{\} functions\.", functions\.size\(\)\);\s+\}',
           analyse_logic + '\n\n    std::sort(functions.begin(), functions.end(), [](auto& lhs, auto& rhs) { return lhs.base < rhs.base; });\n    fmt::println("Analysis complete. Found {} functions.", functions.size());\n}')

# SaveCurrentOutData optimization
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'out\.clear\(\);',
           'std::string().swap(out);')

# Recompile signature fix (if needed, but let's just fix the NULL warnings first)
# replace NULL with 0 in recompiler_config.cpp and recompiler.cpp
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler_config.cpp', r'== NULL', '== 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'!= NULL', '!= 0')
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp', r'== NULL', '== 0')

# Fix fread warning
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'fread\(temp\.data\(\), 1, fileSize, f\);',
           'if (fread(temp.data(), 1, fileSize, f) != fileSize) { fclose(f); return; }')

# 5. xbox.h and xdbf.h anonymous aggregates
patch_file('tools/XenonRecomp/XenonUtils/xbox.h',
           r'struct\s+\{\s+be<uint32_t> Error;\s+be<uint32_t> Length;\s+\};',
           'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')

patch_file('tools/XenonRecomp/XenonUtils/xdbf.h',
           r'struct\s+\{\s+be<uint16_t> u16;\s+char u8\[0x02\];\s+\};',
           'struct { be<uint16_t> u16; char u8[0x02]; } _s2;')

print("Patching complete.")
