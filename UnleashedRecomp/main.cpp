#include <stdafx.h>
#ifdef __ANDROID__
#include <os/android/perf_android.h>
#endif
#ifdef __x86_64__
#include <cpuid.h>
#endif
#include <cpu/guest_thread.h>
#include <gpu/video.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <file.h>
#include <xex.h>
#include <apu/audio.h>
#include <hid/hid.h>
#include <user/config.h>
#include <user/paths.h>
#include <user/persistent_storage_manager.h>
#include <user/registry.h>
#include <kernel/xdbf.h>
#include <install/installer.h>
#include <install/update_checker.h>
#include <os/logger.h>
#include <os/process.h>
#include <os/registry.h>
#include <ui/game_window.h>
#include <ui/installer_wizard.h>
#include <mod/mod_loader.h>
#include <preload_executable.h>

const size_t XMAIOBegin = 0x7FEA0000;
const size_t XMAIOEnd = XMAIOBegin + 0x0000FFFF;

Memory g_memory;
Heap g_userHeap;
XDBFWrapper g_xdbfWrapper;
std::unordered_map<uint16_t, GuestTexture*> g_xdbfTextureCache;

void HostStartup()
{
    hid::Init();
}

// Name inspired from nt's entry point
void KiSystemStartup()
{
    if (g_memory.base == nullptr)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
        std::_Exit(1);
    }

    g_userHeap.Init();

    const auto gameContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Game");
    const auto updateContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Update");
    const std::string gamePath = (const char*)(GetGamePath() / "game").u8string().c_str();
    const std::string updatePath = (const char*)(GetGamePath() / "update").u8string().c_str();
    XamRegisterContent(gameContent, gamePath);
    XamRegisterContent(updateContent, updatePath);

    const auto saveFilePath = GetSaveFilePath(true);
    bool saveFileExists = std::filesystem::exists(saveFilePath);

    if (!saveFileExists)
    {
        // Copy base save data to modded save as fallback.
        std::error_code ec;
        std::filesystem::create_directories(saveFilePath.parent_path(), ec);

        if (!ec)
        {
            std::filesystem::copy_file(GetSaveFilePath(false), saveFilePath, ec);
            saveFileExists = !ec;
        }
    }

    if (saveFileExists)
    {
        std::u8string savePathU8 = saveFilePath.parent_path().u8string();
        XamRegisterContent(XamMakeContent(XCONTENTTYPE_SAVEDATA, "SYS-DATA"), (const char*)(savePathU8.c_str()));
    }

    // Mount game
    XamContentCreateEx(0, "game", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
    XamContentCreateEx(0, "update", &updateContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    // OS mounts game data to D:
    XamContentCreateEx(0, "D", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    std::error_code ec;
    for (auto& file : std::filesystem::directory_iterator(GetGamePath() / "dlc", ec))
    {
        if (file.is_directory())
        {
#ifdef _WIN32
            std::u8string fileNameU8 = file.path().filename().u8string();
            std::u8string filePathU8 = file.path().u8string();
            XamRegisterContent(XamMakeContent(XCONTENTTYPE_DLC, (const char*)(fileNameU8.c_str())), (const char*)(filePathU8.c_str()));
#else
            XamRegisterContent(XamMakeContent(XCONTENTTYPE_DLC, file.path().filename().c_str()), file.path().c_str());
#endif
        }
    }

    XAudioInitializeSystem();

    PreloadContext preloadContext;
    preloadContext.PreloadExecutable();
}

uint32_t LdrLoadModule(const std::filesystem::path &path)
{
    auto loadResult = LoadFile(path);
    if (loadResult.empty())
    {
        assert("Failed to load module" && false);
        return 0;
    }

    auto* header = reinterpret_cast<const Xex2Header*>(loadResult.data());
    auto* security = reinterpret_cast<const Xex2SecurityInfo*>(loadResult.data() + header->securityOffset);
    const auto* fileFormatInfo = reinterpret_cast<const Xex2OptFileFormatInfo*>(getOptHeaderPtr(loadResult.data(), XEX_HEADER_FILE_FORMAT_INFO));
    auto entry = *reinterpret_cast<const uint32_t*>(getOptHeaderPtr(loadResult.data(), XEX_HEADER_ENTRY_POINT));
    ByteSwapInplace(entry);

    auto srcData = loadResult.data() + header->headerSize;
    auto destData = reinterpret_cast<uint8_t*>(g_memory.Translate(security->loadAddress));
    auto loadAddress = security->loadAddress;

    if ((uint64_t)loadAddress + security->imageSize > PPC_MEMORY_SIZE)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
        std::_Exit(1);
    }

    if (fileFormatInfo->compressionType == XEX_COMPRESSION_NONE)
    {
        memcpy(destData, srcData, security->imageSize);
    }
    else if (fileFormatInfo->compressionType == XEX_COMPRESSION_BASIC)
    {
        if (fileFormatInfo->infoSize < sizeof(Xex2FileBasicCompressionInfo))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
            std::_Exit(1);
        }

        auto* blocks = reinterpret_cast<const Xex2FileBasicCompressionBlock*>(fileFormatInfo + 1);
        const size_t numBlocks = (fileFormatInfo->infoSize / sizeof(Xex2FileBasicCompressionInfo)) - 1;

        auto* currentDest = destData;

        for (size_t i = 0; i < numBlocks; i++)
        {
            if (blocks[i].dataSize > 0)
            {
                if (!g_memory.IsInMemoryRange(currentDest) || !g_memory.IsInMemoryRange(currentDest + blocks[i].dataSize - 1))
                {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
                    std::_Exit(1);
                }
            }

            memcpy(currentDest, srcData, blocks[i].dataSize);

            srcData += blocks[i].dataSize;
            currentDest += blocks[i].dataSize;

            if (blocks[i].zeroSize > 0)
            {
                if (!g_memory.IsInMemoryRange(currentDest) || !g_memory.IsInMemoryRange(currentDest + blocks[i].zeroSize - 1))
                {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
                    std::_Exit(1);
                }
            }

            memset(currentDest, 0, blocks[i].zeroSize);
            currentDest += blocks[i].zeroSize;
        }
    }
    else
    {
        assert(false && "Unknown compression type.");
    }

    auto res = reinterpret_cast<const Xex2ResourceInfo*>(getOptHeaderPtr(loadResult.data(), XEX_HEADER_RESOURCE_INFO));

    g_xdbfWrapper = XDBFWrapper((uint8_t*)g_memory.Translate(res->offset.get()), res->sizeOfData);

    return entry;
}

#ifdef __x86_64__
__attribute__((constructor(101), target("no-avx,no-avx2"), noinline))
void init()
{
    uint32_t eax, ebx, ecx, edx;

    // Execute CPUID for processor info and feature bits.
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    // Check for AVX support.
    if ((ecx & (1 << 28)) == 0)
    {
        printf("[*] CPU does not support the AVX instruction set.\n");

        std::_Exit(1);
    }
}
#endif

struct CommandLineOptions
{
    bool ForceInstaller = false;
    bool ForceDLCInstaller = false;
    bool UseDefaultWorkingDirectory = false;
    bool ForceInstallationCheck = false;
    bool GraphicsApiRetry = false;
    const char* SdlVideoDriver = nullptr;
};

CommandLineOptions ParseCommandLineArguments(int argc, char* argv[])
{
    CommandLineOptions options;

    for (int i = 1; i < argc; i++)
    {
        options.ForceInstaller = options.ForceInstaller || (strcmp(argv[i], "--install") == 0);
        options.ForceDLCInstaller = options.ForceDLCInstaller || (strcmp(argv[i], "--install-dlc") == 0);
        options.UseDefaultWorkingDirectory = options.UseDefaultWorkingDirectory || (strcmp(argv[i], "--use-cwd") == 0);
        options.ForceInstallationCheck = options.ForceInstallationCheck || (strcmp(argv[i], "--install-check") == 0);
        options.GraphicsApiRetry = options.GraphicsApiRetry || (strcmp(argv[i], "--graphics-api-retry") == 0);

        if (strcmp(argv[i], "--sdl-video-driver") == 0)
        {
            if ((i + 1) < argc)
                options.SdlVideoDriver = argv[++i];
            else
                LOGN_WARNING("No argument was specified for --sdl-video-driver. Option will be ignored.");
        }
    }

    return options;
}

void InitializeSystem()
{
#ifdef __ANDROID__
    SDL_setenv("SDL_AUDIO_DRIVER", "aaudio", 1);
    SDL_setenv("SDL_VIDEODRIVER", "android", 1);

    perf::EnableSustainedPerformanceMode(true);
#endif

    os::process::CheckConsole();

    if (!os::registry::Init())
        LOGN_WARNING("OS does not support registry.");

    os::logger::Init();
}

void SetWorkingDirectory(const CommandLineOptions& options)
{
    if (!options.UseDefaultWorkingDirectory)
    {
        // Set the current working directory to the executable's path.
        std::error_code ec;
        std::filesystem::current_path(os::process::GetExecutableRoot(), ec);
    }
}

void PerformInstallationIntegrityCheck(const CommandLineOptions& options)
{
    if (options.ForceInstallationCheck)
    {
        // Create the console to show progress to the user, otherwise it will seem as if the game didn't boot at all.
        os::process::ShowConsole();

        Journal journal;
        double lastProgressMiB = 0.0;
        double lastTotalMib = 0.0;
        Installer::checkInstallIntegrity(GAME_INSTALL_DIRECTORY, journal, [&]()
        {
            constexpr double MiBDivisor = 1024.0 * 1024.0;
            constexpr double MiBProgressThreshold = 128.0;
            double progressMiB = double(journal.progressCounter) / MiBDivisor;
            double totalMiB = double(journal.progressTotal) / MiBDivisor;
            if (journal.progressCounter > 0)
            {
                if ((progressMiB - lastProgressMiB) > MiBProgressThreshold)
                {
                    LOGFN("Checking files: {:.2f} MiB / {:.2f} MiB", progressMiB, totalMiB);
                    lastProgressMiB = progressMiB;
                }
            }
            else
            {
                if ((totalMiB - lastTotalMib) > MiBProgressThreshold)
                {
                    LOGFN("Scanning files: {:.2f} MiB", totalMiB);
                    lastTotalMib = totalMiB;
                }
            }

            return true;
        });

        char resultText[512];
        uint32_t messageBoxStyle;
        if (journal.lastResult == Journal::Result::Success)
        {
            snprintf(resultText, sizeof(resultText), "%s", Localise("IntegrityCheck_Success").c_str());
            LOGN(resultText);
            messageBoxStyle = SDL_MESSAGEBOX_INFORMATION;
        }
        else
        {
            snprintf(resultText, sizeof(resultText), "%s", fmt::format(fmt::runtime(Localise("IntegrityCheck_Failed")), journal.lastErrorMessage).c_str());
            LOGN_ERROR(resultText);
            messageBoxStyle = SDL_MESSAGEBOX_ERROR;
        }

        SDL_ShowSimpleMessageBox(messageBoxStyle, GameWindow::GetTitle(), resultText, GameWindow::s_pWindow);
        std::_Exit(int(journal.lastResult));
    }
}

void CheckForUpdates()
{
    // Check the time since the last time an update was checked. Store the new time if the difference is more than six hours.
    constexpr double TimeBetweenUpdateChecksInSeconds = 6 * 60 * 60;
    time_t timeNow = std::time(nullptr);
    double timeDifferenceSeconds = difftime(timeNow, Config::LastChecked);
    if (timeDifferenceSeconds > TimeBetweenUpdateChecksInSeconds)
    {
        UpdateChecker::initialize();
        UpdateChecker::start();
        Config::LastChecked = timeNow;
        Config::Save();
    }
}

void InitializeVideoBackend(const CommandLineOptions& options)
{
    if (!Video::CreateHostDevice(options.SdlVideoDriver, options.GraphicsApiRetry))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("Video_BackendError").c_str(), GameWindow::s_pWindow);
        std::_Exit(1);
    }
}

bool RunInstallerIfNeeded(const CommandLineOptions& options, std::filesystem::path& modulePath)
{
    bool isGameInstalled = Installer::checkGameInstall(GetGamePath(), modulePath);
    bool runInstallerWizard = options.ForceInstaller || options.ForceDLCInstaller || !isGameInstalled;

    if (runInstallerWizard)
    {
        InitializeVideoBackend(options);

        if (!InstallerWizard::Run(GetGamePath(), isGameInstalled && options.ForceDLCInstaller))
        {
            std::_Exit(0);
        }
    }

    return runInstallerWizard;
}

int main(int argc, char *argv[])
{
    InitializeSystem();

    PreloadContext preloadContext;
    preloadContext.PreloadExecutable();

    CommandLineOptions options = ParseCommandLineArguments(argc, argv);

    SetWorkingDirectory(options);

    Config::Load();

    PerformInstallationIntegrityCheck(options);

    CheckForUpdates();

    if (Config::ShowConsole)
        os::process::ShowConsole();

    HostStartup();

    std::filesystem::path modulePath;
    bool installerRan = RunInstallerIfNeeded(options, modulePath);

    ModLoader::Init();

    if (!PersistentStorageManager::LoadBinary())
        LOGFN_ERROR("Failed to load persistent storage binary... (status code {})", (int)PersistentStorageManager::BinStatus);

    KiSystemStartup();

    uint32_t entry = LdrLoadModule(modulePath);

    if (!installerRan)
    {
        InitializeVideoBackend(options);
    }

    Video::StartPipelinePrecompilation();

    GuestThread::Start({ entry, 0, 0 });

    return 0;
}

GUEST_FUNCTION_STUB(__imp__vsprintf);
GUEST_FUNCTION_STUB(__imp___vsnprintf);
GUEST_FUNCTION_STUB(__imp__sprintf);
GUEST_FUNCTION_STUB(__imp___snprintf);
GUEST_FUNCTION_STUB(__imp___snwprintf);
GUEST_FUNCTION_STUB(__imp__vswprintf);
GUEST_FUNCTION_STUB(__imp___vscwprintf);
GUEST_FUNCTION_STUB(__imp__swprintf);
