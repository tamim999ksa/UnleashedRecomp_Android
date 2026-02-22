// Referenced from: https://github.com/xenia-canary/xenia-canary/blob/canary_experimental/src/xenia/vfs/devices/disc_image_device.cc

/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "iso_file_system.h"

#include <cstring>
#include <deque>
#include <stack>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

ISOFileSystem::ISOFileSystem(const std::filesystem::path &isoPath)
{
    mappedFile.open(isoPath);
    if (!mappedFile.isOpen())
    {
        return;
    }

    name = (const char *)(isoPath.filename().u8string().data());

    uint32_t gameOffset = 0;
    if (!findRootOffset(gameOffset))
    {
        mappedFile.close();
        return;
    }

    uint32_t rootSector = 0;
    uint32_t rootSize = 0;
    if (!parseRootInformation(gameOffset, rootSector, rootSize))
    {
        mappedFile.close();
        return;
    }

    parseDirectoryTree(gameOffset, rootSector, rootSize);
}

bool ISOFileSystem::load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const
{
    auto it = fileMap.find(path);
    if (it != fileMap.end())
    {
        if (fileDataMaxByteCount < std::get<1>(it->second))
        {
            return false;
        }

        size_t offset = std::get<0>(it->second);
        size_t length = std::get<1>(it->second);
        if ((offset + length < offset) || ((offset + length) > mappedFile.size()))
        {
            return false;
        }

        const uint8_t *mappedFileData = mappedFile.data();
        memcpy(fileData, &mappedFileData[offset], length);
        return true;
    }
    else
    {
        return false;
    }
}

size_t ISOFileSystem::getSize(const std::string &path) const
{
    auto it = fileMap.find(path);
    if (it != fileMap.end())
    {
        return std::get<1>(it->second);
    }
    else
    {
        return 0;
    }
}

bool ISOFileSystem::exists(const std::string &path) const
{
    return fileMap.find(path) != fileMap.end();
}

const std::string &ISOFileSystem::getName() const
{
    return name;
}

bool ISOFileSystem::empty() const
{
    return !mappedFile.isOpen();
}

std::unique_ptr<ISOFileSystem> ISOFileSystem::create(const std::filesystem::path &isoPath) {
    std::unique_ptr<ISOFileSystem> isoFs = std::make_unique<ISOFileSystem>(isoPath);
    if (!isoFs->empty())
    {
        return isoFs;
    }
    else
    {
        return nullptr;
    }
}

bool ISOFileSystem::findRootOffset(uint32_t &gameOffset)
{
    const uint8_t *mappedFileData = mappedFile.data();
    gameOffset = 0;
    static const size_t PossibleOffsets[] = { 0x00000000, 0x0000FB20, 0x00020600, 0x02080000, 0x0FD90000, };
    bool magicFound = false;
    const char RefMagic[] = "MICROSOFT*XBOX*MEDIA";
    for (size_t i = 0; i < std::size(PossibleOffsets); i++)
    {
        size_t fileOffset = PossibleOffsets[i] + (32 * XeSectorSize);
        if ((fileOffset + strlen(RefMagic)) > mappedFile.size())
        {
            continue;
        }

        if (std::memcmp(&mappedFileData[fileOffset], RefMagic, strlen(RefMagic)) == 0)
        {
            gameOffset = PossibleOffsets[i];
            magicFound = true;
        }
    }
    return magicFound;
}

bool ISOFileSystem::parseRootInformation(uint32_t gameOffset, uint32_t &rootSector, uint32_t &rootSize)
{
    size_t rootInfoOffset = gameOffset + (32 * XeSectorSize) + 20;
    if ((rootInfoOffset + 8) > mappedFile.size())
    {
        return false;
    }

    const uint8_t *mappedFileData = mappedFile.data();
    rootSector = *(uint32_t *)(&mappedFileData[rootInfoOffset + 0]);
    rootSize = *(uint32_t *)(&mappedFileData[rootInfoOffset + 4]);

    const uint32_t MinRootSize = 13;
    const uint32_t MaxRootSize = 32 * 1024 * 1024;
    if ((rootSize < MinRootSize) || (rootSize > MaxRootSize))
    {
        return false;
    }

    return true;
}

void ISOFileSystem::parseDirectoryTree(uint32_t gameOffset, uint32_t rootSector, uint32_t rootSize)
{
    struct IterationStep
    {
        std::string_view fileNameBase;
        size_t nodeOffset = 0;
        size_t entryOffset = 0;

        IterationStep() = default;
        IterationStep(std::string_view fileNameBase, size_t nodeOffset, size_t entryOffset) : fileNameBase(fileNameBase), nodeOffset(nodeOffset), entryOffset(entryOffset) { }
    };

    size_t rootOffset = gameOffset + (rootSector * XeSectorSize);

    std::deque<std::string> pathCache;
    pathCache.emplace_back("");

    std::stack<IterationStep> iterationStack;
    iterationStack.emplace(pathCache.back(), rootOffset, 0);

    std::unordered_set<size_t> visitedOffsets;
    const uint8_t *mappedFileData = mappedFile.data();

    IterationStep step;
    uint16_t nodeL, nodeR;
    uint32_t sector, length;
    uint8_t attributes, nameLength;
    char fileName[256];
    const uint8_t FileAttributeDirectory = 0x10;
    while (!iterationStack.empty())
    {
        step = iterationStack.top();
        iterationStack.pop();

        size_t infoOffset = step.nodeOffset + step.entryOffset;
        if ((infoOffset + 14) > mappedFile.size())
        {
            mappedFile.close();
            return;
        }

        if (visitedOffsets.count(infoOffset))
        {
            continue;
        }
        visitedOffsets.insert(infoOffset);

        nodeL = *(uint16_t *)(&mappedFileData[infoOffset + 0]);
        nodeR = *(uint16_t *)(&mappedFileData[infoOffset + 2]);
        sector = *(uint32_t *)(&mappedFileData[infoOffset + 4]);
        length = *(uint32_t *)(&mappedFileData[infoOffset + 8]);
        attributes = *(uint8_t *)(&mappedFileData[infoOffset + 12]);
        nameLength = *(uint8_t *)(&mappedFileData[infoOffset + 13]);

        size_t nameOffset = infoOffset + 14;
        if ((nameOffset + nameLength) > mappedFile.size())
        {
            mappedFile.close();
            return;
        }

        memcpy(fileName, &mappedFileData[nameOffset], nameLength);
        fileName[nameLength] = '\0';

        if (nodeL)
        {
            iterationStack.emplace(step.fileNameBase, step.nodeOffset, nodeL * 4);
        }

        if (nodeR)
        {
            iterationStack.emplace(step.fileNameBase, step.nodeOffset, nodeR * 4);
        }

        std::string fileNameUTF8;
        fileNameUTF8.reserve(step.fileNameBase.length() + nameLength);
        fileNameUTF8.append(step.fileNameBase);
        fileNameUTF8.append(fileName, nameLength);

        if (attributes & FileAttributeDirectory)
        {
            if (length > 0)
            {
                pathCache.emplace_back(std::move(fileNameUTF8)).push_back('/');
                iterationStack.emplace(pathCache.back(), gameOffset + sector * XeSectorSize, 0);
            }
        }
        else
        {
            fileMap.try_emplace(std::move(fileNameUTF8), gameOffset + sector * XeSectorSize, length);
        }
    }
}
