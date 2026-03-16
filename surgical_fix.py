import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    if not os.path.exists(path):
        print(f"Skipping {path} (not found)")
        return
    with open(path, 'r') as f: content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(path, 'w') as f: f.write(new_content)

# 1. symbol.h - make size mutable for range established in Analyse
patch_file('tools/XenonRecomp/XenonUtils/symbol.h', r'size_t size\{\};', 'mutable size_t size{};')

# 2. symbol_table.h - Robust multi-step range lookup
# We rewrite the find methods to be range-aware
new_find_logic = """    const_iterator find(size_t address) const
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
           new_find_logic)

# 3. function.cpp - tail call fix and min size guarantee
# Fix tail call check and add min size logic at the end
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'r') as f: content = f.read()
# Tail call check
content = content.replace(
    'if (*((uint32_t*)code + 1) == 0x04000048) // shifted ptr tail call',
    'if (size >= 8 && ByteSwap(((const uint32_t*)code)[1]) == 0x04000048) // shifted ptr tail call'
)
# Min size guarantee before return
pos = content.rfind('return fn;')
if pos != -1:
    content = content[:pos] + 'if (fn.size == 0 && data > (const uint32_t*)code) fn.size = 4; ' + content[pos:]
with open('tools/XenonRecomp/XenonAnalyse/function.cpp', 'w') as f: f.write(content)

# 4. recompiler.cpp - OOM and Loop fixes
with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'r') as f: content = f.read()

# Increase reservation
content = content.replace('functions.reserve(8192);', 'functions.reserve(100000);')

# Analyse sequential scan logic - establishments and skip
old_scan = """        while (data < dataEnd)
        {
            auto invalidInstr = config.invalidInstructions.find(ByteSwap(*(uint32_t*)data));
            if (invalidInstr != config.invalidInstructions.end())
            {
                base += invalidInstr->second;
                data += invalidInstr->second;
                continue;
            }

            auto& fn = functions.emplace_back(Function::Analyze(data, dataEnd - data, base));
            image.symbols.emplace(fmt::format("sub_{:X}", fn.base), fn.base, fn.size, Symbol_Function);

            size_t advanced = std::max<size_t>(fn.size, 4);
            base += advanced;
            data += advanced;

            if ((base % 0x10000) == 0)
            {
                fmt::println("Scanning section... {:X} / {:X}", base, dataEnd - section.data + section.base);
            }
        }"""

new_scan = """        // Establish ranges from pre-defined symbols
        for (auto it = image.symbols.lower_bound(section.base); it != image.symbols.end() && it->address < section.base + section.size; ++it)
        {
            if (it->type == Symbol_Function)
            {
                auto data_ptr = section.data + it->address - section.base;
                auto& fn = functions.emplace_back(Function::Analyze(data_ptr, section.base + section.size - it->address, it->address));
                it->size = fn.size;
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
                fmt::println("Scanning section... {:X} / {:X} (Found {} functions)", base, section.base + section.size, functions.size());
            }
        }"""
content = content.replace(old_scan, new_scan)

# Memory optimization in SaveCurrentOutData
content = content.replace('out.clear();', 'std::string().swap(out);')

with open('tools/XenonRecomp/XenonRecomp/recompiler.cpp', 'w') as f: f.write(content)

print("Surgical fixes applied.")
