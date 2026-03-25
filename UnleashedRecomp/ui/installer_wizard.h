#pragma once

#include <api/SWA.h>

#ifdef __ANDROID__
#include <SDL.h>
#include <user/paths.h>
#include <sstream>

struct InstallerWizard
{
    static inline bool s_isVisible = false;

    static inline void Init() {}
    static inline void Draw() {}
    static inline void Shutdown() {}
    static inline bool Run(std::filesystem::path installPath, bool skipGame)
    {
        std::ostringstream msg;
        msg << "Game files not found.\n\n"
            << "Please copy your game files to:\n"
            << installPath.string() << "\n\n"
            << "Required structure:\n"
            << "  game/default.xex\n"
            << "  game/*.ar.00 (game archives)\n"
            << "  update/default.xexp\n"
            << "  dlc/ (optional)\n\n"
            << "Use a file manager to place the files, then restart the app.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unleashed Recompiled", msg.str().c_str(), NULL);
        return false;
    }
};
#else
struct InstallerWizard
{
    static inline bool s_isVisible = false;

    static void Init();
    static void Draw();
    static void Shutdown();
    static bool Run(std::filesystem::path installPath, bool skipGame);
};
#endif
