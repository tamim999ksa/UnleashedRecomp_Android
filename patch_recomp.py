import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    with open(path, 'r') as f:
        content = f.read()

    new_content = re.sub(pattern, replacement, content, flags=flags)

    with open(path, 'w') as f:
        f.write(new_content)

# 1. SymbolTable range lookup
symbol_table_find_const = """    const_iterator find(size_t address) const
    {
        if (empty())
        {
            return end();
        }

        auto it = upper_bound(address);
        if (it == begin())
        {
            return end();
        }

        auto curr = std::prev(it);
        // Check current and a few previous symbols for containing range or exact match
        for (int i = 0; i < 32; ++i)
        {
            if (address >= curr->address && address < curr->address + curr->size)
            {
                return curr;
            }
            if (address == curr->address)
            {
                return curr;
            }

            if (curr == begin())
            {
                break;
            }
            curr = std::prev(curr);

            // Optimization: if we're too far back, stop
            if (address > curr->address + 1024 * 1024)
            {
                break;
            }
        }

        return end();
    }"""

symbol_table_find_non_const = """    iterator find(size_t address)
    {
        if (empty())
        {
            return end();
        }

        auto it = upper_bound(address);
        if (it == begin())
        {
            return end();
        }

        auto curr = std::prev(it);
        for (int i = 0; i < 32; ++i)
        {
            if (address >= curr->address && address < curr->address + curr->size)
            {
                return curr;
            }
            if (address == curr->address)
            {
                return curr;
            }

            if (curr == begin())
            {
                break;
            }
            curr = std::prev(curr);

            if (address > curr->address + 1024 * 1024)
            {
                break;
            }
        }

        return end();
    }"""

patch_file('tools/XenonRecomp/XenonUtils/symbol_table.h',
           r'const_iterator find\(size_t address\) const.*?\}',
           symbol_table_find_const)

patch_file('tools/XenonRecomp/XenonUtils/symbol_table.h',
           r'iterator find\(size_t address\).*?\}',
           symbol_table_find_non_const)

# 2. Function::Analyze
function_analyze_fix = """Function Function::Analyze(const void* code, size_t size, size_t base)
{
    Function fn{ base, 0 };

    const auto* dataStart = (const uint32_t*)code;
    const auto* dataEnd = (const uint32_t*)((const uint8_t*)code + size);

    // Safety check for shifted ptr tail call
    if (size >= 8 && dataStart[1] == 0x48000004) // ByteSwap(0x04000048)
    {
        fn.size = 0x8;
        return fn;
    }

    auto& blocks = fn.blocks;"""

patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'Function Function::Analyze\(const void\* code, size_t size, size_t base\).*?auto& blocks = fn.blocks;',
           function_analyze_fix)

# Add minimum size to end of Analyze
function_analyze_end_fix = """    // Ensure we always return at least 4 bytes if any data was processed to prevent infinite loops
    if (fn.size == 0 && data > dataStart)
    {
        fn.size = 4;
    }

    return fn;
}"""

patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'return fn;\s+\}',
           function_analyze_end_fix)

# 3. Recompiler::Analyse
recompiler_analyse_fix = """void Recompiler::Analyse()
{
    functions.reserve(50000);
    for (size_t i = 14; i < 128; i++)
    {"""

patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'void Recompiler::Analyse\(\).*?for \(size_t i = 14; i < 128; i\+\+\)\s+\{',
           recompiler_analyse_fix)

# Skip logic in Analyse
recompiler_scan_fix = """            auto fnSymbol = image.symbols.find(base);
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
            }"""

patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'auto fnSymbol = image\.symbols\.find\(base\);.*?base \+= advanced;\s+data \+= advanced;\s+\}',
           recompiler_scan_fix)

# 4. Recompiler::SaveCurrentOutData
save_fix = """        if (shouldWrite)
        {
            f = fopen(filePath.c_str(), "wb");
            fwrite(out.data(), 1, out.size(), f);
            fclose(f);
        }
        std::string().swap(out);
    }
}"""

patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'if \(shouldWrite\).*?out\.clear\(\);\s+\}\s+\}',
           save_fix)

# 5. recompiler.h signature Fix
# Ensure it has Recompile(const RecompileArgs& args) and matches recompiler.cpp
# Recompiler::Recompile(const RecompileArgs& args) is already in recompiler.cpp
# Let's check recompiler.h again
