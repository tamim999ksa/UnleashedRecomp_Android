#include "pch.h"
#include "recompiler.h"
#include <xex_patcher.h>

static uint64_t ComputeMask(uint32_t mstart, uint32_t mstop)
{
    mstart &= 0x3F;
    mstop &= 0x3F;
    uint64_t value = (UINT64_MAX >> mstart) ^ ((mstop >= 63) ? 0 : UINT64_MAX >> (mstop + 1));
    return mstart <= mstop ? value : ~value;
}

bool Recompiler::LoadConfig(const std::string_view& configFilePath)
{
    config.Load(configFilePath);

    std::vector<uint8_t> file;
    if (!config.patchedFilePath.empty())
        file = LoadFile((config.directoryPath + config.patchedFilePath).c_str());

    if (file.empty())
    {
        file = LoadFile((config.directoryPath + config.filePath).c_str());

        if (!config.patchFilePath.empty())
        {
            const auto patchFile = LoadFile((config.directoryPath + config.patchFilePath).c_str());
            if (!patchFile.empty())
            {
                std::vector<uint8_t> outBytes;
                auto result = XexPatcher::apply(file.data(), file.size(), patchFile.data(), patchFile.size(), outBytes, false);
                if (result == XexPatcher::Result::Success)
                {
                    std::exchange(file, outBytes);

                    if (!config.patchedFilePath.empty())
                    {
                        std::ofstream stream(config.directoryPath + config.patchedFilePath, std::ios::binary);
                        if (stream.good())
                        {
                            stream.write(reinterpret_cast<const char*>(file.data()), file.size());
                            stream.close();
                        }
                    }
                }
                else
                {
                    fmt::print("ERROR: Unable to apply the patch file, ");

                    switch (result)
                    {
                    case XexPatcher::Result::XexFileUnsupported:
                        fmt::println("XEX file unsupported");
                        break;

                    case XexPatcher::Result::XexFileInvalid:
                        fmt::println("XEX file invalid");
                        break;

                    case XexPatcher::Result::PatchFileInvalid:
                        fmt::println("patch file invalid");
                        break;

                    case XexPatcher::Result::PatchIncompatible:
                        fmt::println("patch file incompatible");
                        break;

                    case XexPatcher::Result::PatchFailed:
                        fmt::println("patch failed");
                        break;

                    case XexPatcher::Result::PatchUnsupported:
                        fmt::println("patch unsupported");
                        break;

                    default:
                        fmt::println("reason unknown");
                        break;
                    }

                    return false;
                }
            }
            else
            {
                fmt::println("ERROR: Unable to load the patch file");
                return false;
            }
        }
    }

    image = Image::ParseImage(file.data(), file.size());
    return true;
}

void Recompiler::Analyse()
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

        if (i >= 64)
        {
            if (config.restVmx64Address != 0)
            {
                auto& restvmx = functions.emplace_back();
                restvmx.base = config.restVmx64Address + (i - 64) * 8;
                restvmx.size = (128 - i) * 8 + 4;
                image.symbols.emplace(fmt::format("__restvmx_{}", i), restvmx.base, restvmx.size, Symbol_Function);
            }

            if (config.saveVmx64Address != 0)
            {
                auto& savevmx = functions.emplace_back();
                savevmx.base = config.saveVmx64Address + (i - 64) * 8;
                savevmx.size = (128 - i) * 8 + 4;
                image.symbols.emplace(fmt::format("__savevmx_{}", i), savevmx.base, savevmx.size, Symbol_Function);
            }
        }
    }

    for (auto& [address, size] : config.functions)
    {
        functions.emplace_back(address, size);
        image.symbols.emplace(fmt::format("sub_{:X}", address), address, size, Symbol_Function);
    }

    auto& pdata = *image.Find(".pdata");
    size_t count = pdata.size / sizeof(IMAGE_CE_RUNTIME_FUNCTION);
    auto* pf = (IMAGE_CE_RUNTIME_FUNCTION*)pdata.data;
    for (size_t i = 0; i < count; i++)
    {
        auto fn = pf[i];
        fn.BeginAddress = ByteSwap(fn.BeginAddress);
        fn.Data = ByteSwap(fn.Data);

        if (image.symbols.find(fn.BeginAddress) == image.symbols.end())
        {
            auto& f = functions.emplace_back();
            f.base = fn.BeginAddress;
            f.size = fn.FunctionLength * 4;

            image.symbols.emplace(fmt::format("sub_{:X}", f.base), f.base, f.size, Symbol_Function);
        }
    }

    for (const auto& section : image.sections)
    {
        if (!(section.flags & SectionFlags_Code))
        {
            continue;
        }
        size_t base = section.base;
        uint8_t* data = section.data;
        uint8_t* dataEnd = section.data + section.size;

        while (data < dataEnd)
        {
            uint32_t insn = ByteSwap(*(uint32_t*)data);
            if (PPC_OP(insn) == PPC_OP_B && PPC_BL(insn))
            {
                size_t address = base + (data - section.data) + PPC_BI(insn);

                if (address >= section.base && address < section.base + section.size && (image.symbols.find(address) == image.symbols.end() || image.symbols.find(address)->type != Symbol_Function))
                {
                    auto data = section.data + address - section.base;
                    auto& fn = functions.emplace_back(Function::Analyze(data, section.base + section.size - address, address));
                    image.symbols.emplace(fmt::format("sub_{:X}", fn.base), fn.base, fn.size, Symbol_Function);
                }
            }
            data += 4;
        }

        data = section.data;

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
                fmt::println("Scanning section... {:X} / {:X}", base, dataEnd - section.data + section.base);
            }
        }
    }

    std::sort(functions.begin(), functions.end(), [](auto& lhs, auto& rhs) { return lhs.base < rhs.base; });
    fmt::println("Analysis complete. Found {} functions.", functions.size());
}

bool Recompiler::Recompile(const RecompileArgs& args)
{
    const Function& fn = args.fn;
    uint32_t base = args.base;
    const ppc_insn& insn = args.insn;
    const uint32_t* data = args.data;
    std::unordered_map<uint32_t, RecompilerSwitchTable>::iterator& switchTable = args.switchTable;
    RecompilerLocalVariables& localVariables = args.localVariables;
    CSRState& csrState = args.csrState;

    println("\t// {} {}", insn.opcode->name, insn.op_str);

    auto r = [&](size_t index)
        {
            if ((config.nonArgumentRegistersAsLocalVariables && (index == 0 || index == 2 || index == 11 || index == 12)) ||
                (config.nonVolatileRegistersAsLocalVariables && index >= 14))
            {
                localVariables.r[index] = true;
                return fmt::format("r{}", index);
            }
            return fmt::format("ctx.r{}", index);
        };

    auto f = [&](size_t index)
        {
            if ((config.nonArgumentRegistersAsLocalVariables && index == 0) ||
                (config.nonVolatileRegistersAsLocalVariables && index >= 14))
            {
                localVariables.f[index] = true;
                return fmt::format("f{}", index);
            }
            return fmt::format("ctx.f{}", index);
        };

    auto v = [&](size_t index)
        {
            if ((config.nonArgumentRegistersAsLocalVariables && (index >= 32 && index <= 63)) ||
                (config.nonVolatileRegistersAsLocalVariables && ((index >= 14 && index <= 31) || (index >= 64 && index <= 127))))
            {
                localVariables.v[index] = true;
                return fmt::format("v{}", index);
            }
            return fmt::format("ctx.v{}", index);
        };

    auto cr = [&](size_t index)
        {
            if (config.crRegistersAsLocalVariables)
            {
                localVariables.cr[index] = true;
                return fmt::format("cr{}", index);
            }
            return fmt::format("ctx.cr{}", index);
        };

    auto ctr = [&]()
        {
            if (config.ctrAsLocalVariable)
            {
                localVariables.ctr = true;
                return "ctr";
            }
            return "ctx.ctr";
        };

    auto xer = [&]()
        {
            if (config.xerAsLocalVariable)
            {
                localVariables.xer = true;
                return "xer";
            }
            return "ctx.xer";
        };

    auto reserved = [&]()
        {
            if (config.reservedRegisterAsLocalVariable)
            {
                localVariables.reserved = true;
                return "reserved";
            }
            return "ctx.reserved";
        };

    auto temp = [&]()
        {
            localVariables.temp = true;
            return "temp";
        };

    auto vTemp = [&]()
        {
            localVariables.vTemp = true;
            return "vTemp";
        };

    auto env = [&]()
        {
            localVariables.env = true;
            return "env";
        };

    auto ea = [&]()
        {
            localVariables.ea = true;
            return "ea";
        };

    auto mmioStore = [&]() -> bool
        {
            return *(data + 1) == c_eieio;
        };

    auto printFunctionCall = [&](uint32_t address)
        {
            if (address == config.longJmpAddress)
            {
                println("\tlongjmp(*reinterpret_cast<jmp_buf*>(base + {}.u32), {}.s32);", r(3), r(4));
            }
            else if (address == config.setJmpAddress)
            {
                println("\t{} = ctx;", env());
                println("\t{}.s64 = setjmp(*reinterpret_cast<jmp_buf*>(base + {}.u32));", temp(), r(3));
                println("\tif ({}.s64 != 0) ctx = {};", temp(), env());
                println("\t{} = {};", r(3), temp());
            }
            else
            {
                auto targetSymbol = image.symbols.find(address);

                if (targetSymbol != image.symbols.end() && targetSymbol->address == address && targetSymbol->type == Symbol_Function)
                {
                    if (config.nonVolatileRegistersAsLocalVariables && (targetSymbol->name.find("__rest") == 0 || targetSymbol->name.find("__save") == 0))
                    {
                    }
                    else
                    {
                        println("\t{}(ctx, base);", targetSymbol->name);
                    }
                }
                else
                {
                    println("\tPPC_FUNC_CALL(0x{:X});", address);
                }
            }
        };

    auto printMidAsmHook = [&]()
        {
            auto midAsmHook = config.midAsmHooks.find(base);

            if (midAsmHook->second.returnOnFalse || midAsmHook->second.returnOnTrue ||
                midAsmHook->second.jumpAddressOnFalse != 0 || midAsmHook->second.jumpAddressOnTrue != 0)
            {
                print("\tif ({} (", midAsmHook->second.name);
            }
            else
            {
                print("\t{} (", midAsmHook->second.name);
            }

            for (auto& reg : midAsmHook->second.registers)
            {
                if (out.back() != '(')
                    out += ", ";

                switch (reg[0])
                {
                case 'c':
                    if (reg == "ctr")
                        print("{}", ctr());
                    else
                        print("{}", cr(reg.back() - '0'));
                    break;

                case 'x':
                    print("{}", xer());
                    break;

                case 'r':
                {
                    char* end;
                    size_t index = std::strtoull(&reg[1], &end, 10);
                    print("{}", r(index));
                    break;
                }

                case 'f':
                {
                    char* end;
                    size_t index = std::strtoull(&reg[1], &end, 10);
                    print("{}", f(index));
                    break;
                }

                case 'v':
                {
                    char* end;
                    size_t index = std::strtoull(&reg[1], &end, 10);
                    print("{}", v(index));
                    break;
                }
                }
            }

            if (midAsmHook->second.returnOnFalse)
            {
                println(") == false) return;");
            }
            else if (midAsmHook->second.returnOnTrue)
            {
                println(") == true) return;");
            }
            else if (midAsmHook->second.jumpAddressOnTrue != 0)
            {
                println(") == true) goto loc_{:X};", midAsmHook->second.jumpAddressOnTrue);
            }
            else if (midAsmHook->second.jumpAddressOnFalse != 0)
            {
                println(") == false) goto loc_{:X};", midAsmHook->second.jumpAddressOnFalse);
            }
            else
            {
                println(");");
                if (midAsmHook->second.ret)
                    println("\treturn;");
                else if (midAsmHook->second.jumpAddress != 0)
                    println("\tgoto loc_{:X};", midAsmHook->second.jumpAddress);
            }
        };

    auto midAsmHook = config.midAsmHooks.find(base);
    if (midAsmHook != config.midAsmHooks.end() && !midAsmHook->second.afterInstruction)
        printMidAsmHook();

    const uint32_t instruction = ByteSwap(*data);
    const uint32_t op = PPC_OP(instruction);
    const uint32_t xop = PPC_XOP(instruction);

    switch (insn.opcode->id)
    {
    case PPC_INST_ADD:
        println("\t{} = {}.u64 + {}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_ADDC:
        println("\t{} = {}.u64 + {}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.ca = {}.u64 < {}.u64;", xer(), r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_ADDE:
        println("\t{} = {}.u64 + {}.u64 + {}.ca;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value), xer());
        println("\t{}.ca = (({}.u64 == {}.u64) && {}.ca) || ({}.u64 < {}.u64);", xer(), r(insn.operands[0].value), r(insn.operands[1].value), xer(), r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_ADDG6S:
        println("\t{}.u64 = ({}.u64 & 0xF) + ({}.u64 & 0xF);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_ADDI:
        if (insn.operands[1].value == 0)
            println("\t{} = 0x{:X}ull;", r(insn.operands[0].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        else
            println("\t{} = {}.u64 + 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        break;

    case PPC_INST_ADDIC:
        println("\t{} = {}.u64 + 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        println("\t{}.ca = {}.u64 < {}.u64;", xer(), r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_ADDICR:
        println("\t{} = {}.u64 + 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        println("\t{}.ca = {}.u64 < {}.u64;", xer(), r(insn.operands[0].value), r(insn.operands[1].value));
        println("\tUpdateCR0({}, {}.u64);", cr(0), r(insn.operands[0].value));
        break;

    case PPC_INST_ADDIS:
        if (insn.operands[1].value == 0)
            println("\t{} = 0x{:X}ull;", r(insn.operands[0].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value)) << 16));
        else
            println("\t{} = {}.u64 + 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value)) << 16));
        break;

    case PPC_INST_ADDME:
        println("\t{} = {}.u64 + {}.ca - 1;", r(insn.operands[0].value), r(insn.operands[1].value), xer());
        println("\t{}.ca = {}.u64 != 0 || {}.ca != 0;", xer(), r(insn.operands[1].value), xer());
        break;

    case PPC_INST_ADDZE:
        println("\t{} = {}.u64 + {}.ca;", r(insn.operands[0].value), r(insn.operands[1].value), xer());
        println("\t{}.ca = {}.u64 < {}.u64;", xer(), r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_AND:
        println("\t{} = {}.u64 & {}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_ANDC:
        println("\t{} = {}.u64 & ~{}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_ANDI:
        println("\t{} = {}.u64 & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value));
        println("\tUpdateCR0({}, {}.u64);", cr(0), r(insn.operands[0].value));
        break;

    case PPC_INST_ANDIS:
        println("\t{} = {}.u64 & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value << 16));
        println("\tUpdateCR0({}, {}.u64);", cr(0), r(insn.operands[0].value));
        break;

    case PPC_INST_B:
        if (PPC_BL(instruction))
        {
            printFunctionCall(base + PPC_BI(instruction));
        }
        else
        {
            println("\tgoto loc_{:X};", base + PPC_BI(instruction));
        }
        break;

    case PPC_INST_BC:
    {
        uint32_t bo = PPC_BO(instruction);
        std::string condition;

        if (!(bo & 4))
        {
            println("\t--{}.u64;", ctr());
            condition = fmt::format("{}.u64 {} 0", ctr(), (bo & 2) ? "!=" : "==");
        }

        if (!(bo & 0x10))
        {
            if (!condition.empty())
                condition += " && ";

            condition += fmt::format("{}.{} == {}", cr(insn.operands[0].value >> 2),
                ConditionFields[insn.operands[0].value & 3], (bo & 8) ? 1 : 0);
        }

        if (PPC_BL(instruction))
        {
            if (!condition.empty())
                println("\tif ({})", condition);

            printFunctionCall(base + PPC_BD(instruction));
        }
        else
        {
            if (!condition.empty())
                println("\tif ({}) goto loc_{:X};", condition, base + PPC_BD(instruction));
            else
                println("\tgoto loc_{:X};", base + PPC_BD(instruction));
        }
        break;
    }

    case PPC_INST_BCCTR:
    {
        uint32_t bo = PPC_BO(instruction);
        std::string condition;

        if (!(bo & 0x10))
        {
            condition = fmt::format("{}.{} == {}", cr(insn.operands[0].value >> 2),
                ConditionFields[insn.operands[0].value & 3], (bo & 8) ? 1 : 0);
        }

        if (PPC_BL(instruction))
        {
            if (!condition.empty())
                println("\tif ({})", condition);

            printFunctionCall(ctr());
        }
        else
        {
            if (!condition.empty())
                println("\tif ({})", condition);

            println("\tPPC_BCCTR({}.u64);", ctr());
        }
        break;
    }

    case PPC_INST_BCL:
        printFunctionCall(base + PPC_BD(instruction));
        break;

    case PPC_INST_BCLR:
    {
        uint32_t bo = PPC_BO(instruction);
        std::string condition;

        if (!(bo & 4))
        {
            println("\t--{}.u64;", ctr());
            condition = fmt::format("{}.u64 {} 0", ctr(), (bo & 2) ? "!=" : "==");
        }

        if (!(bo & 0x10))
        {
            if (!condition.empty())
                condition += " && ";

            condition += fmt::format("{}.{} == {}", cr(insn.operands[0].value >> 2),
                ConditionFields[insn.operands[0].value & 3], (bo & 8) ? 1 : 0);
        }

        if (PPC_BL(instruction))
        {
            if (!condition.empty())
                println("\tif ({})", condition);

            printFunctionCall(fmt::format("ctx.lr.u64", base + PPC_BD(instruction)));
        }
        else
        {
            if (!condition.empty())
                println("\tif ({}) return;", condition);
            else
                println("\treturn;");
        }
        break;
    }

    case PPC_INST_BCTR:
        if (switchTable != config.switchTables.end() && switchTable->first == base)
        {
            println("\tswitch ({}.u32) {{", r(switchTable->second.r));
            for (size_t i = 0; i < switchTable->second.labels.size(); i++)
            {
                println("\tcase {}: goto loc_{:X};", i, switchTable->second.labels[i]);
            }
            println("\tdefault: break;");
            println("\t}}");
        }
        else
        {
            println("\tPPC_BCTR({}.u64);", ctr());
        }
        break;

    case PPC_INST_CMP:
        println("\t{} = Compare({}.s64, {}.s64);", cr(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_CMPI:
        println("\t{} = Compare({}.s64, 0x{:X}ll);", cr(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        break;

    case PPC_INST_CMPL:
        println("\t{} = Compare({}.u64, {}.u64);", cr(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_CMPLI:
        println("\t{} = Compare({}.u64, 0x{:X}ull);", cr(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value));
        break;

    case PPC_INST_CNTLZD:
        println("\t{} = CountLeadingZeros({}.u64);", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_CNTLZW:
        println("\t{} = CountLeadingZeros({}.u32);", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_CRAND:
        println("\t{}.{} = {}.{} & {}.{};", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CRANDC:
        println("\t{}.{} = {}.{} & !{}.{};", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CREQV:
        println("\t{}.{} = {}.{} == {}.{};", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CRNAND:
        println("\t{}.{} = !({}.{} & {}.{});", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CRNOR:
        println("\t{}.{} = !({}.{} | {}.{});", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CROR:
        println("\t{}.{} = {}.{} | {}.{};", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CRORC:
        println("\t{}.{} = {}.{} | !{}.{};", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_CRXOR:
        println("\t{}.{} = {}.{} ^ {}.{};", cr(insn.operands[0].value >> 2), ConditionFields[insn.operands[0].value & 3],
            cr(insn.operands[1].value >> 2), ConditionFields[insn.operands[1].value & 3],
            cr(insn.operands[2].value >> 2), ConditionFields[insn.operands[2].value & 3]);
        break;

    case PPC_INST_DIVD:
        println("\t{} = Divide({}.s64, {}.s64);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_DIVDU:
        println("\t{} = Divide({}.u64, {}.u64);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_DIVW:
        println("\t{} = Divide({}.s32, {}.s32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_DIVWU:
        println("\t{} = Divide({}.u32, {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_EIEIO:
        break;

    case PPC_INST_EQV:
        println("\t{} = ~({}.u64 ^ {}.u64);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_EXTSB:
        println("\t{} = static_cast<int64_t>(static_cast<int8_t>({}.u8));", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_EXTSH:
        println("\t{} = static_cast<int64_t>(static_cast<int16_t>({}.u16));", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_EXTSW:
        println("\t{} = static_cast<int64_t>(static_cast<int32_t>({}.u32));", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_FABS:
        println("\t{}.f64 = std::abs({}.f64);", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FADD:
        println("\t{}.f64 = {}.f64 + {}.f64;", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FADDS:
        println("\t{}.f64 = static_cast<double>(static_cast<float>({}.f64) + static_cast<float>({}.f64));", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FCMPU:
    case PPC_INST_FCMPO:
        println("\t{} = Compare({}.f64, {}.f64);", cr(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FCTID:
        println("\t{}.s64 = static_cast<int64_t>({}.f64);", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FCTIDZ:
        println("\t{}.s64 = static_cast<int64_t>({}.f64);", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FCTIW:
        println("\t{}.s32[0] = static_cast<int32_t>({}.f64);", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FCTIWZ:
        println("\t{}.s32[0] = static_cast<int32_t>({}.f64);", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FDIV:
        println("\t{}.f64 = {}.f64 / {}.f64;", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FDIVS:
        println("\t{}.f64 = static_cast<double>(static_cast<float>({}.f64) / static_cast<float>({}.f64));", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FMR:
        println("\t{} = {};", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FMUL:
        println("\t{}.f64 = {}.f64 * {}.f64;", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FMULS:
        println("\t{}.f64 = static_cast<double>(static_cast<float>({}.f64) * static_cast<float>({}.f64));", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FNEG:
        println("\t{}.f64 = -{}.f64;", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FRSP:
        println("\t{}.f64 = static_cast<double>(static_cast<float>({}.f64));", f(insn.operands[0].value), f(insn.operands[1].value));
        break;

    case PPC_INST_FSEL:
        println("\t{}.f64 = ({}.f64 >= 0.0) ? {}.f64 : {}.f64;", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value), f(insn.operands[3].value));
        break;

    case PPC_INST_FSUB:
        println("\t{}.f64 = {}.f64 - {}.f64;", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_FSUBS:
        println("\t{}.f64 = static_cast<double>(static_cast<float>({}.f64) - static_cast<float>({}.f64));", f(insn.operands[0].value), f(insn.operands[1].value), f(insn.operands[2].value));
        break;

    case PPC_INST_ISYNC:
        break;

    case PPC_INST_LBZ:
        println("\t{} = Load<uint8_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LBZU:
        println("\t{} = Load<uint8_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LBZUX:
        println("\t{} = Load<uint8_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LBZX:
        println("\t{} = Load<uint8_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LD:
        println("\t{} = Load<uint64_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LDU:
        println("\t{} = Load<uint64_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LDUX:
        println("\t{} = Load<uint64_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LDX:
        println("\t{} = Load<uint64_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LFD:
        println("\t{}.u64 = Load<uint64_t>(base + {}.u32 + 0x{:X}ull);", f(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LFDU:
        println("\t{}.u64 = Load<uint64_t>(base + {}.u32 + 0x{:X}ull);", f(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LFDUX:
        println("\t{}.u64 = Load<uint64_t>(base + {}.u32 + {}.u32);", f(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LFDX:
        println("\t{}.u64 = Load<uint64_t>(base + {}.u32 + {}.u32);", f(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LFS:
        println("\t{}.f64 = static_cast<double>(Load<float>(base + {}.u32 + 0x{:X}ull));", f(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LFSU:
        println("\t{}.f64 = static_cast<double>(Load<float>(base + {}.u32 + 0x{:X}ull));", f(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LFSUX:
        println("\t{}.f64 = static_cast<double>(Load<float>(base + {}.u32 + {}.u32));", f(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LFSX:
        println("\t{}.f64 = static_cast<double>(Load<float>(base + {}.u32 + {}.u32));", f(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LHA:
        println("\t{} = static_cast<int64_t>(Load<int16_t>(base + {}.u32 + 0x{:X}ull));", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LHAU:
        println("\t{} = static_cast<int64_t>(Load<int16_t>(base + {}.u32 + 0x{:X}ull));", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LHAUX:
        println("\t{} = static_cast<int64_t>(Load<int16_t>(base + {}.u32 + {}.u32));", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LHAX:
        println("\t{} = static_cast<int64_t>(Load<int16_t>(base + {}.u32 + {}.u32));", r(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LHZ:
        println("\t{} = Load<uint16_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LHZU:
        println("\t{} = Load<uint16_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LHZUX:
        println("\t{} = Load<uint16_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LHZX:
        println("\t{} = Load<uint16_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LI:
        println("\t{} = 0x{:X}ull;", r(insn.operands[0].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LIS:
        println("\t{} = 0x{:X}ull;", r(insn.operands[0].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value)) << 16));
        break;

    case PPC_INST_LMID:
        println("\t{} = Load<uint64_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LMW:
        for (uint32_t i = insn.operands[0].value; i < 32; i++)
        {
            println("\t{} = Load<uint32_t>(base + {}.u32 + 0x{:X}ull);", r(i), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value)) + (i - insn.operands[0].value) * 4));
        }
        break;

    case PPC_INST_LVEBX:
        println("\t{}.u8[0] = Load<uint8_t>(base + {}.u32 + {}.u32);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVEHX:
        println("\t{}.u16[0] = Load<uint16_t>(base + {}.u32 + {}.u32);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVEWX:
        println("\t{}.u32[0] = Load<uint32_t>(base + {}.u32 + {}.u32);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVSL:
        println("\t{} = VectorShiftLeft({}.u32, {}.u32);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVSR:
        println("\t{} = VectorShiftRight({}.u32, {}.u32);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVX:
        println("\t{} = Load<PPCVRegister>(base + ({}.u32 + {}.u32) & ~0xF);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVX128:
        println("\t{} = Load<PPCVRegister>(base + {}.u32 + {}.u32);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LVXL:
        println("\t{} = Load<PPCVRegister>(base + ({}.u32 + {}.u32) & ~0xF);", v(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LWARX:
        println("\t{} = Load<uint32_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        println("\t{}.reserved = {}.u32 + {}.u32;", reserved(), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_LWZ:
        println("\t{} = Load<uint32_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LWZU:
        println("\t{} = Load<uint32_t>(base + {}.u32 + 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_LWZUX:
        println("\t{} = Load<uint32_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_LWZX:
        println("\t{} = Load<uint32_t>(base + {}.u32 + {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value));
        break;

    case PPC_INST_MFCR:
        println("\t{}.u64 = static_cast<uint64_t>(", r(insn.operands[0].value));
        for (size_t i = 0; i < 8; i++)
        {
            println("\t\t(static_cast<uint32_t>({}.gt) << {}) |", cr(i), 31 - i * 4);
            println("\t\t(static_cast<uint32_t>({}.lt) << {}) |", cr(i), 31 - (i * 4 + 1));
            println("\t\t(static_cast<uint32_t>({}.eq) << {}) |", cr(i), 31 - (i * 4 + 2));
            println("\t\t(static_cast<uint32_t>({}.so) << {}){}", cr(i), 31 - (i * 4 + 3), i == 7 ? "" : " |");
        }
        println("\t);");
        break;

    case PPC_INST_MFCTR:
        println("\t{} = {};", r(insn.operands[0].value), ctr());
        break;

    case PPC_INST_MFLR:
        println("\t{} = ctx.lr;", r(insn.operands[0].value));
        break;

    case PPC_INST_MFOCRF:
    {
        uint32_t fxm = insn.operands[1].value;
        uint32_t crField = 0;
        for (uint32_t i = 0; i < 8; i++)
        {
            if (fxm & (0x80 >> i))
            {
                crField = i;
                break;
            }
        }
        uint32_t shift = (7 - crField) * 4;
        println("\t{}.u64 = static_cast<uint64_t>(", r(insn.operands[0].value));
        println("\t\t(static_cast<uint32_t>({}.gt) << {}) |", cr(crField), shift + 3);
        println("\t\t(static_cast<uint32_t>({}.lt) << {}) |", cr(crField), shift + 2);
        println("\t\t(static_cast<uint32_t>({}.eq) << {}) |", cr(crField), shift + 1);
        println("\t\t(static_cast<uint32_t>({}.so) << {})", cr(crField), shift);
        println("\t);");
        break;
    }

    case PPC_INST_MFSPR:
        if (insn.operands[1].value == 1) // xer
        {
            println("\t{}.u64 = static_cast<uint64_t>(", r(insn.operands[0].value));
            println("\t\t(static_cast<uint32_t>({}.so) << 31) |", xer());
            println("\t\t(static_cast<uint32_t>({}.ov) << 30) |", xer());
            println("\t\t(static_cast<uint32_t>({}.ca) << 29) |", xer());
            println("\t\tstatic_cast<uint32_t>({}.count)", xer());
            println("\t);");
        }
        else
        {
            fmt::println("Unrecognized SPR at 0x{:X}: {}", base, insn.operands[1].value);
        }
        break;

    case PPC_INST_MFXER:
        println("\t{}.u64 = static_cast<uint64_t>(", r(insn.operands[0].value));
        println("\t\t(static_cast<uint32_t>({}.so) << 31) |", xer());
        println("\t\t(static_cast<uint32_t>({}.ov) << 30) |", xer());
        println("\t\t(static_cast<uint32_t>({}.ca) << 29) |", xer());
        println("\t\tstatic_cast<uint32_t>({}.count)", xer());
        println("\t);");
        break;

    case PPC_INST_MR:
        println("\t{} = {};", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_MTCRF:
    {
        uint32_t crm = insn.operands[0].value;
        for (size_t i = 0; i < 8; i++)
        {
            if (crm & (0x80 >> i))
            {
                println("\t{}.gt = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - i * 4);
                println("\t{}.lt = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - (i * 4 + 1));
                println("\t{}.eq = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - (i * 4 + 2));
                println("\t{}.so = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - (i * 4 + 3));
            }
        }
        break;
    }

    case PPC_INST_MTCTR:
        println("\t{} = {};", ctr(), r(insn.operands[0].value));
        break;

    case PPC_INST_MTLR:
        println("\tctx.lr = {};", r(insn.operands[0].value));
        break;

    case PPC_INST_MTOCRF:
    {
        uint32_t fxm = insn.operands[0].value;
        for (size_t i = 0; i < 8; i++)
        {
            if (fxm & (0x80 >> i))
            {
                println("\t{}.gt = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - i * 4);
                println("\t{}.lt = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - (i * 4 + 1));
                println("\t{}.eq = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - (i * 4 + 2));
                println("\t{}.so = ({}.u64 >> {}) & 1;", cr(i), r(insn.operands[1].value), 31 - (i * 4 + 3));
            }
        }
        break;
    }

    case PPC_INST_MTSPR:
        if (insn.operands[0].value == 1) // xer
        {
            println("\t{}.so = ({}.u64 >> 31) & 1;", xer(), r(insn.operands[1].value));
            println("\t{}.ov = ({}.u64 >> 30) & 1;", xer(), r(insn.operands[1].value));
            println("\t{}.ca = ({}.u64 >> 29) & 1;", xer(), r(insn.operands[1].value));
            println("\t{}.count = {}.u64 & 0x1FFFFFFF;", xer(), r(insn.operands[1].value));
        }
        else
        {
            fmt::println("Unrecognized SPR at 0x{:X}: {}", base, insn.operands[0].value);
        }
        break;

    case PPC_INST_MTXER:
        println("\t{}.so = ({}.u64 >> 31) & 1;", xer(), r(insn.operands[1].value));
        println("\t{}.ov = ({}.u64 >> 30) & 1;", xer(), r(insn.operands[1].value));
        println("\t{}.ca = ({}.u64 >> 29) & 1;", xer(), r(insn.operands[1].value));
        println("\t{}.count = {}.u64 & 0x1FFFFFFF;", xer(), r(insn.operands[1].value));
        break;

    case PPC_INST_MULHD:
        println("\t{} = MultiplyHigh({}.s64, {}.s64);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_MULHDU:
        println("\t{} = MultiplyHigh({}.u64, {}.u64);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_MULHW:
        println("\t{} = MultiplyHigh({}.s32, {}.s32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_MULHWU:
        println("\t{} = MultiplyHigh({}.u32, {}.u32);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_MULLD:
        println("\t{} = {}.u64 * {}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_MULLI:
        println("\t{} = {}.u64 * 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        break;

    case PPC_INST_MULLW:
        println("\t{} = static_cast<int64_t>(static_cast<int32_t>({}.u32) * static_cast<int32_t>({}.u32));", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_NEG:
        println("\t{} = -{}.s64;", r(insn.operands[0].value), r(insn.operands[1].value));
        break;

    case PPC_INST_NOP:
        break;

    case PPC_INST_NOR:
        println("\t{} = ~({}.u64 | {}.u64);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_OR:
        println("\t{} = {}.u64 | {}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_ORC:
        println("\t{} = {}.u64 | ~{}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_ORI:
        println("\t{} = {}.u64 | 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value));
        break;

    case PPC_INST_ORIS:
        println("\t{} = {}.u64 | 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value << 16));
        break;

    case PPC_INST_RLDIC:
        println("\t{} = RotateLeft({}.u64, {}) & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), insn.operands[2].value, ComputeMask(0, 63 - insn.operands[2].value));
        break;

    case PPC_INST_RLDICL:
        println("\t{} = RotateLeft({}.u64, {}) & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), insn.operands[2].value, ComputeMask(insn.operands[3].value, 63));
        break;

    case PPC_INST_RLDICR:
        println("\t{} = RotateLeft({}.u64, {}) & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), insn.operands[2].value, ComputeMask(0, insn.operands[3].value));
        break;

    case PPC_INST_RLDIMI:
    {
        uint64_t mask = ComputeMask(insn.operands[3].value, 63 - insn.operands[2].value);
        println("\t{} = ({}.u64 & ~0x{:X}ull) | (RotateLeft({}.u64, {}) & 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[0].value), mask, r(insn.operands[1].value), insn.operands[2].value, mask);
        break;
    }

    case PPC_INST_RLWNM:
        println("\t{} = RotateLeft({}.u32, {}.u8[0] & 0x1F) & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value), ComputeMask(insn.operands[3].value + 32, insn.operands[4].value + 32));
        break;

    case PPC_INST_RLWINM:
        println("\t{} = RotateLeft({}.u32, {}) & 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), insn.operands[2].value, ComputeMask(insn.operands[3].value + 32, insn.operands[4].value + 32));
        break;

    case PPC_INST_RLWIMI:
    {
        uint64_t mask = ComputeMask(insn.operands[3].value + 32, insn.operands[4].value + 32);
        println("\t{} = ({}.u32 & ~0x{:X}ull) | (RotateLeft({}.u32, {}) & 0x{:X}ull);", r(insn.operands[0].value), r(insn.operands[0].value), mask, r(insn.operands[1].value), insn.operands[2].value, mask);
        break;
    }

    case PPC_INST_SC:
        println("\tPPC_SYSCALL();");
        break;

    case PPC_INST_SLD:
        println("\t{} = {}.u64 << ({}.u8[0] & 0x7F);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SLW:
        println("\t{} = static_cast<uint64_t>({}.u32 << ({}.u8[0] & 0x3F));", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SRAD:
        println("\t{} = {}.s64 >> ({}.u8[0] & 0x7F);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SRADI:
        println("\t{} = {}.s64 >> {};", r(insn.operands[0].value), r(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_SRAW:
        println("\t{} = static_cast<int64_t>({}.s32 >> ({}.u8[0] & 0x3F));", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SRAWI:
        println("\t{} = static_cast<int64_t>({}.s32 >> {});", r(insn.operands[0].value), r(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_SRD:
        println("\t{} = {}.u64 >> ({}.u8[0] & 0x7F);", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SRW:
        println("\t{} = static_cast<uint64_t>({}.u32 >> ({}.u8[0] & 0x3F));", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STB:
        println("\tStore<uint8_t>(base + {}.u32 + 0x{:X}ull, {}.u8);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        break;

    case PPC_INST_STBU:
        println("\tStore<uint8_t>(base + {}.u32 + 0x{:X}ull, {}.u8);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_STBUX:
        println("\tStore<uint8_t>(base + {}.u32 + {}.u32, {}.u8);", r(insn.operands[1].value), r(insn.operands[2].value), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STBX:
        println("\tStore<uint8_t>(base + {}.u32 + {}.u32, {}.u8);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), r(insn.operands[0].value));
        break;

    case PPC_INST_STD:
        println("\tStore<uint64_t>(base + {}.u32 + 0x{:X}ull, {}.u64);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        break;

    case PPC_INST_STDU:
        println("\tStore<uint64_t>(base + {}.u32 + 0x{:X}ull, {}.u64);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_STDUX:
        println("\tStore<uint64_t>(base + {}.u32 + {}.u32, {}.u64);", r(insn.operands[1].value), r(insn.operands[2].value), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STDX:
        println("\tStore<uint64_t>(base + {}.u32 + {}.u32, {}.u64);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), r(insn.operands[0].value));
        break;

    case PPC_INST_STFD:
        println("\tStore<uint64_t>(base + {}.u32 + 0x{:X}ull, {}.u64);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), f(insn.operands[0].value));
        break;

    case PPC_INST_STFDU:
        println("\tStore<uint64_t>(base + {}.u32 + 0x{:X}ull, {}.u64);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), f(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_STFDUX:
        println("\tStore<uint64_t>(base + {}.u32 + {}.u32, {}.u64);", r(insn.operands[1].value), r(insn.operands[2].value), f(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STFDX:
        println("\tStore<uint64_t>(base + {}.u32 + {}.u32, {}.u64);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), f(insn.operands[0].value));
        break;

    case PPC_INST_STFIWX:
        println("\tStore<uint32_t>(base + {}.u32 + {}.u32, {}.u32[0]);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), f(insn.operands[0].value));
        break;

    case PPC_INST_STFS:
        println("\tStore<float>(base + {}.u32 + 0x{:X}ull, static_cast<float>({}.f64));", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), f(insn.operands[0].value));
        break;

    case PPC_INST_STFSU:
        println("\tStore<float>(base + {}.u32 + 0x{:X}ull, static_cast<float>({}.f64));", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), f(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_STFSUX:
        println("\tStore<float>(base + {}.u32 + {}.u32, static_cast<float>({}.f64));", r(insn.operands[1].value), r(insn.operands[2].value), f(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STFSX:
        println("\tStore<float>(base + {}.u32 + {}.u32, static_cast<float>({}.f64));", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), f(insn.operands[0].value));
        break;

    case PPC_INST_STH:
        println("\tStore<uint16_t>(base + {}.u32 + 0x{:X}ull, {}.u16);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        break;

    case PPC_INST_STHU:
        println("\tStore<uint16_t>(base + {}.u32 + 0x{:X}ull, {}.u16);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_STHUX:
        println("\tStore<uint16_t>(base + {}.u32 + {}.u32, {}.u16);", r(insn.operands[1].value), r(insn.operands[2].value), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STHX:
        println("\tStore<uint16_t>(base + {}.u32 + {}.u32, {}.u16);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), r(insn.operands[0].value));
        break;

    case PPC_INST_STMW:
        for (uint32_t i = insn.operands[0].value; i < 32; i++)
        {
            println("\tStore<uint32_t>(base + {}.u32 + 0x{:X}ull, {}.u32);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value)) + (i - insn.operands[0].value) * 4), r(i));
        }
        break;

    case PPC_INST_STVEBX:
        println("\tStore<uint8_t>(base + {}.u32 + {}.u32, {}.u8[0]);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), v(insn.operands[0].value));
        break;

    case PPC_INST_STVEHX:
        println("\tStore<uint16_t>(base + {}.u32 + {}.u32, {}.u16[0]);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), v(insn.operands[0].value));
        break;

    case PPC_INST_STVEWX:
        println("\tStore<uint32_t>(base + {}.u32 + {}.u32, {}.u32[0]);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), v(insn.operands[0].value));
        break;

    case PPC_INST_STVX:
        println("\tStore<PPCVRegister>(base + ({}.u32 + {}.u32) & ~0xF, {});", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), v(insn.operands[0].value));
        break;

    case PPC_INST_STVX128:
        println("\tStore<PPCVRegister>(base + {}.u32 + {}.u32, {});", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), v(insn.operands[0].value));
        break;

    case PPC_INST_STVXL:
        println("\tStore<PPCVRegister>(base + ({}.u32 + {}.u32) & ~0xF, {});", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), v(insn.operands[0].value));
        break;

    case PPC_INST_STW:
        println("\tStore<uint32_t>(base + {}.u32 + 0x{:X}ull, {}.u32);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        break;

    case PPC_INST_STWCX:
        println("\t{}.eq = StoreConditional<uint32_t>(base + {}.u32 + {}.u32, {}.reserved, {}.u32);", cr(0), r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), reserved(), r(insn.operands[0].value));
        break;

    case PPC_INST_STWU:
        println("\tStore<uint32_t>(base + {}.u32 + 0x{:X}ull, {}.u32);", r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + 0x{:X}ull;", r(insn.operands[2].value), r(insn.operands[2].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[1].value))));
        break;

    case PPC_INST_STWUX:
        println("\tStore<uint32_t>(base + {}.u32 + {}.u32, {}.u32);", r(insn.operands[1].value), r(insn.operands[2].value), r(insn.operands[0].value));
        println("\t{}.u64 = {}.u32 + {}.u32;", r(insn.operands[1].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_STWX:
        println("\tStore<uint32_t>(base + {}.u32 + {}.u32, {}.u32);", r(insn.operands[1].value == 0 ? "0" : r(insn.operands[1].value)), r(insn.operands[2].value), r(insn.operands[0].value));
        break;

    case PPC_INST_SUBF:
        println("\t{} = {}.u64 - {}.u64;", r(insn.operands[0].value), r(insn.operands[2].value), r(insn.operands[1].value));
        break;

    case PPC_INST_SUBFC:
        println("\t{} = {}.u64 - {}.u64;", r(insn.operands[0].value), r(insn.operands[2].value), r(insn.operands[1].value));
        println("\t{}.ca = {}.u64 <= {}.u64;", xer(), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SUBFE:
        println("\t{} = {}.u64 - {}.u64 + {}.ca - 1;", r(insn.operands[0].value), r(insn.operands[2].value), r(insn.operands[1].value), xer());
        println("\t{}.ca = (({}.u64 < {}.u64) && {}.ca) || ({}.u64 <= {}.u64);", xer(), r(insn.operands[2].value), r(insn.operands[1].value), xer(), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_SUBFIC:
        println("\t{} = 0x{:X}ull - {}.u64;", r(insn.operands[0].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))), r(insn.operands[1].value));
        println("\t{}.ca = {}.u64 <= 0x{:X}ull;", xer(), r(insn.operands[1].value), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(insn.operands[2].value))));
        break;

    case PPC_INST_SUBFME:
        println("\t{} = {}.u64 + {}.ca - 2;", r(insn.operands[0].value), r(insn.operands[1].value), xer());
        println("\t{}.ca = {}.u64 > 1 || ({}.u64 == 1 && {}.ca != 0);", xer(), r(insn.operands[1].value), xer());
        break;

    case PPC_INST_SUBFZE:
        println("\t{} = {}.ca - {}.u64 - 1;", r(insn.operands[0].value), r(insn.operands[1].value), xer());
        println("\t{}.ca = {}.u64 == 0 && {}.ca != 0;", xer(), r(insn.operands[1].value), xer());
        break;

    case PPC_INST_SYNC:
        break;

    case PPC_INST_TD:
        break;

    case PPC_INST_TDI:
        break;

    case PPC_INST_TWI:
        break;

    case PPC_INST_VADDUBM:
        println("\t{} = VectorAdd<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VADDUHM:
        println("\t{} = VectorAdd<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VADDUWM:
        println("\t{} = VectorAdd<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VAND:
        println("\t{} = VectorAnd({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VANDC:
        println("\t{} = VectorAndC({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VCFSX:
        println("\t{} = VectorConvertFromSigned({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VCFUX:
        println("\t{} = VectorConvertFromUnsigned({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VCTSXS:
        println("\t{} = VectorConvertToSigned({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VCTUXS:
        println("\t{} = VectorConvertToUnsigned({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VEXPTEFP:
        println("\t{} = VectorExponent({}.f32);", v(insn.operands[0].value), v(insn.operands[1].value));
        break;

    case PPC_INST_VLOGEFP:
        println("\t{} = VectorLog({}.f32);", v(insn.operands[0].value), v(insn.operands[1].value));
        break;

    case PPC_INST_VMAXFP:
        println("\t{} = VectorMax<float>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMAXSH:
        println("\t{} = VectorMax<int16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMAXSW:
        println("\t{} = VectorMax<int32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMAXUB:
        println("\t{} = VectorMax<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMAXUH:
        println("\t{} = VectorMax<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMAXUW:
        println("\t{} = VectorMax<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMINFP:
        println("\t{} = VectorMin<float>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMINSH:
        println("\t{} = VectorMin<int16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMINSW:
        println("\t{} = VectorMin<int32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMINUB:
        println("\t{} = VectorMin<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMINUH:
        println("\t{} = VectorMin<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMINUW:
        println("\t{} = VectorMin<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMSUMSHM:
        println("\t{} = VectorMultiplySum<int16_t>({}, {}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value), v(insn.operands[3].value));
        break;

    case PPC_INST_VMSUMUHM:
        println("\t{} = VectorMultiplySum<uint16_t>({}, {}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value), v(insn.operands[3].value));
        break;

    case PPC_INST_VMULESH:
        println("\t{} = VectorMultiplyEven<int16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMULEUH:
        println("\t{} = VectorMultiplyEven<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMULOSH:
        println("\t{} = VectorMultiplyOdd<int16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VMULOUH:
        println("\t{} = VectorMultiplyOdd<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VNOR:
        println("\t{} = VectorNor({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VOR:
        println("\t{} = VectorOr({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VREFP:
        println("\t{} = VectorReciprocal({}.f32);", v(insn.operands[0].value), v(insn.operands[1].value));
        break;

    case PPC_INST_VRLH:
        println("\t{} = VectorRotateLeft<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VRLW:
        println("\t{} = VectorRotateLeft<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VRSQRTEFP:
        println("\t{} = VectorReciprocalSquareRoot({}.f32);", v(insn.operands[0].value), v(insn.operands[1].value));
        break;

    case PPC_INST_VSEL:
        println("\t{} = VectorSelect({}, {}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value), v(insn.operands[3].value));
        break;

    case PPC_INST_VSL:
        println("\t{} = VectorShiftLeft({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSLB:
        println("\t{} = VectorShiftLeft<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSLH:
        println("\t{} = VectorShiftLeft<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSLO:
        println("\t{} = VectorShiftLeftOctet({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSLW:
        println("\t{} = VectorShiftLeft<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSPLTB:
        println("\t{} = VectorSplat<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VSPLTH:
        println("\t{} = VectorSplat<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VSPLTW:
        println("\t{} = VectorSplat<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), insn.operands[2].value);
        break;

    case PPC_INST_VSPLTISB:
        println("\t{} = VectorSplatImmediate<int8_t>({});", v(insn.operands[0].value), static_cast<int32_t>(static_cast<int8_t>(insn.operands[1].value << 3) >> 3));
        break;

    case PPC_INST_VSPLTISH:
        println("\t{} = VectorSplatImmediate<int16_t>({});", v(insn.operands[0].value), static_cast<int32_t>(static_cast<int16_t>(insn.operands[1].value << 11) >> 11));
        break;

    case PPC_INST_VSPLTISW:
        println("\t{} = VectorSplatImmediate<int32_t>({});", v(insn.operands[0].value), static_cast<int32_t>(static_cast<int32_t>(insn.operands[1].value << 27) >> 27));
        break;

    case PPC_INST_VSR:
        println("\t{} = VectorShiftRight({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRAB:
        println("\t{} = VectorShiftRightArithmetic<int8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRAH:
        println("\t{} = VectorShiftRightArithmetic<int16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRAW:
        println("\t{} = VectorShiftRightArithmetic<int32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRB:
        println("\t{} = VectorShiftRightLogical<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRH:
        println("\t{} = VectorShiftRightLogical<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRO:
        println("\t{} = VectorShiftRightOctet({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSRW:
        println("\t{} = VectorShiftRightLogical<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSUBUBM:
        println("\t{} = VectorSubtract<uint8_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSUBUHM:
        println("\t{} = VectorSubtract<uint16_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VSUBUWM:
        println("\t{} = VectorSubtract<uint32_t>({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_VXOR:
        println("\t{} = VectorXor({}, {});", v(insn.operands[0].value), v(insn.operands[1].value), v(insn.operands[2].value));
        break;

    case PPC_INST_XOR:
        println("\t{} = {}.u64 ^ {}.u64;", r(insn.operands[0].value), r(insn.operands[1].value), r(insn.operands[2].value));
        break;

    case PPC_INST_XORI:
        println("\t{} = {}.u64 ^ 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value));
        break;

    case PPC_INST_XORIS:
        println("\t{} = {}.u64 ^ 0x{:X}ull;", r(insn.operands[0].value), r(insn.operands[1].value), static_cast<uint64_t>(insn.operands[2].value << 16));
        break;

    default:
        return false;
    }

    if (midAsmHook != config.midAsmHooks.end() && midAsmHook->second.afterInstruction)
        printMidAsmHook();

    return true;
}

bool Recompiler::Recompile(const Function& fn)
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

            println(");\n");

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
    if (symbol != image.symbols.end())
    {
        name = symbol->name;
    }
    else
    {
        name = fmt::format("sub_{}", fn.base);
    }

#ifdef XENON_RECOMP_USE_ALIAS
    println("__attribute__((alias(\"__imp__{}\"))) PPC_WEAK_FUNC({});", name, name);
#endif

    println("PPC_FUNC_IMPL(__imp__{}) {{", name);
    println("\tPPC_FUNC_PROLOGUE();");

    auto switchTable = config.switchTables.end();
    bool allRecompiled = true;
    CSRState csrState = CSRState::Unknown;

    RecompilerLocalVariables localVariables;
    static std::string tempString;
    tempString.clear();
    std::swap(out, tempString);

    ppc_insn insn;
    while (base < end)
    {
        if (labels.find(base) != labels.end())
        {
            println("loc_{:X}:", base);

            csrState = CSRState::Unknown;
        }

        if (switchTable == config.switchTables.end())
            switchTable = config.switchTables.find(base);

        ppc::Disassemble(data, 4, base, insn);

        if (insn.opcode == nullptr)
        {
            println("\t// {}", insn.op_str);
#if 1
            if (*data != 0)
                fmt::println("Unable to decode instruction {:X} at {:X}", *data, base);
#endif
        }
        else
        {
            if (insn.opcode->id == PPC_INST_BCTR && (*(data - 1) == 0x07008038 || *(data - 1) == 0x00000060) && switchTable == config.switchTables.end())
                fmt::println("Found a switch jump table at {:X} with no switch table entry present", base);

            if (!Recompile({ fn, base, insn, data, switchTable, localVariables, csrState }))
            {
                fmt::println("Unrecognized instruction at 0x{:X}: {}", base, insn.opcode->name);
                allRecompiled = false;
            }
        }

        base += 4;
        ++data;
    }

    println("}}\n");

#ifndef XENON_RECOMP_USE_ALIAS
    println("PPC_WEAK_FUNC({}) {{", name);
    println("\t__imp__{}(ctx, base);", name);
    println("}}\n");
#endif

    std::swap(out, tempString);
    if (localVariables.ctr)
        println("\tPPCRegister ctr{{}};");
    if (localVariables.xer)
        println("\tPPCXERRegister xer{{}};");
    if (localVariables.reserved)
        println("\tPPCRegister reserved{{}};");

    for (size_t i = 0; i < 8; i++)
    {
        if (localVariables.cr[i])
            println("\tPPCCRRegister cr{}{{}};", i);
    }

    for (size_t i = 0; i < 32; i++)
    {
        if (localVariables.r[i])
            println("\tPPCRegister r{}{{}};", i);
    }

    for (size_t i = 0; i < 32; i++)
    {
        if (localVariables.f[i])
            println("\tPPCRegister f{}{{}};", i);
    }

    for (size_t i = 0; i < 128; i++)
    {
        if (localVariables.v[i])
            println("\tPPCVRegister v{}{{}};", i);
    }

    if (localVariables.env)
        println("\tPPCContext env{{}};");

    if (localVariables.temp)
        println("\tPPCRegister temp{{}};");

    if (localVariables.vTemp)
        println("\tPPCVRegister vTemp{{}};");

    if (localVariables.ea)
        println("\tuint32_t ea{{}};");

    out += tempString;

    return allRecompiled;
}

void Recompiler::Recompile(const std::filesystem::path& headerFilePath)
{
    out.reserve(10 * 1024 * 1024);

    {
        println("#pragma once");

        println("#ifndef PPC_CONFIG_H_INCLUDED");
        println("#define PPC_CONFIG_H_INCLUDED\n");

        if (config.skipLr)
            println("#define PPC_CONFIG_SKIP_LR");
        if (config.ctrAsLocalVariable)
            println("#define PPC_CONFIG_CTR_AS_LOCAL");
        if (config.xerAsLocalVariable)
            println("#define PPC_CONFIG_XER_AS_LOCAL");
        if (config.reservedRegisterAsLocalVariable)
            println("#define PPC_CONFIG_RESERVED_AS_LOCAL");
        if (config.skipMsr)
            println("#define PPC_CONFIG_SKIP_MSR");
        if (config.crRegistersAsLocalVariables)
            println("#define PPC_CONFIG_CR_AS_LOCAL");
        if (config.nonArgumentRegistersAsLocalVariables)
            println("#define PPC_CONFIG_NON_ARGUMENT_AS_LOCAL");
        if (config.nonVolatileRegistersAsLocalVariables)
            println("#define PPC_CONFIG_NON_VOLATILE_AS_LOCAL");

        println("");

        println("#define PPC_IMAGE_BASE 0x{:X}ull", image.base);
        println("#define PPC_IMAGE_SIZE 0x{:X}ull", image.size);

        size_t codeMin = ~0;
        size_t codeMax = 0;

        for (auto& section : image.sections)
        {
            if ((section.flags & SectionFlags_Code) != 0)
            {
                if (section.base < codeMin)
                    codeMin = section.base;

                if ((section.base + section.size) > codeMax)
                    codeMax = (section.base + section.size);
            }
        }

        println("#define PPC_CODE_BASE 0x{:X}ull", codeMin);
        println("#define PPC_CODE_SIZE 0x{:X}ull", codeMax - codeMin);

        println("");

        println("#ifdef PPC_INCLUDE_DETAIL");
        println("#include \"ppc_detail.h\"");
        println("#endif");

        println("\n#endif");

        SaveCurrentOutData("ppc_config.h");
    }

    {
        println("#pragma once");

        println("#include \"ppc_config.h\"\n");

        std::ifstream stream(headerFilePath);
        if (stream.good())
        {
            std::stringstream ss;
            ss << stream.rdbuf();
            out += ss.str();
        }

        SaveCurrentOutData("ppc_context.h");
    }

    {
        println("#pragma once\n");
        println("#include \"ppc_config.h\"");
        println("#include \"ppc_context.h\"\n");

        for (auto& symbol : image.symbols)
            println("PPC_EXTERN_FUNC({});", symbol.name);

        SaveCurrentOutData("ppc_recomp_shared.h");
    }

    {
        println("#include \"ppc_recomp_shared.h\"\n");

        println("PPCFuncMapping PPCFuncMappings[] = {{");
        for (auto& symbol : image.symbols)
            println("\t{{ 0x{:X}, {} }},", symbol.address, symbol.name);

        println("\t{{ 0, nullptr }}");
        println("}};");

        SaveCurrentOutData("ppc_func_mapping.cpp");
    }

    for (size_t i = 0; i < functions.size(); i++)
    {
        if ((i % 256) == 0)
        {
            SaveCurrentOutData();
            println("#include \"ppc_recomp_shared.h\"\n");
        }

        if ((i % 2048) == 0 || (i == (functions.size() - 1)))
            fmt::println("Recompiling functions... {}%", static_cast<float>(i + 1) / functions.size() * 100.0f);

        Recompile(functions[i]);
    }

    SaveCurrentOutData();
}

void Recompiler::SaveCurrentOutData(const std::string_view& name)
{
    if (!out.empty())
    {
        std::string cppName;

        if (name.empty())
        {
            cppName = fmt::format("ppc_recomp.{}.cpp", cppFileIndex);
            ++cppFileIndex;
        }

        bool shouldWrite = true;

        std::string directoryPath = config.directoryPath;
        if (!directoryPath.empty())
            directoryPath += "/";

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
}
