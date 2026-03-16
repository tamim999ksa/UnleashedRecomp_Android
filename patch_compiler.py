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

# 2. Function::Analyze - add minimum size guarantee
patch_file('tools/XenonRecomp/XenonAnalyse/function.cpp',
           r'return fn;\s+\}',
           '    // Ensure we always return at least 4 bytes if any data was processed to prevent infinite loops\n    if (fn.size == 0 && data > dataStart) fn.size = 4;\n    return fn;\n}')

# 3. Recompiler::Analyse
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'functions\.reserve\(8192\);',
           'functions.reserve(50000);')

# Recompiler scan logic in Analyse
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

# Use a regex that matches the original while (data < dataEnd) block content
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'auto fnSymbol = image\.symbols\.find\(base\);.*?base \+= advanced;\s+data \+= advanced;\s+\}',
           recompiler_scan_fix)

# 4. Recompiler::SaveCurrentOutData memory optimization
patch_file('tools/XenonRecomp/XenonRecomp/recompiler.cpp',
           r'out\.clear\(\);',
           'std::string().swap(out);')

# 5. GCC 13 Anonymous Aggregate Fixes
patch_file('tools/XenonRecomp/XenonUtils/xbox.h',
           r'struct\s+\{\s+be<uint32_t> Error;\s+be<uint32_t> Length;\s+\};',
           'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')

patch_file('tools/XenonRecomp/XenonUtils/xdbf.h',
           r'struct\s+\{\s+be<uint16_t> u16;\s+char u8\[0x02\];\s+\};',
           'struct { be<uint16_t> u16; char u8[0x02]; } _s2;')

print("Patching complete.")
