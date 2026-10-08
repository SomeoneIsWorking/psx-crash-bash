#include "install_media.h"
#include "user_paths.h"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_messagebox.h>
#include <lucent/log.h>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace crashbash::appimage {
namespace {

struct DialogResult {
  std::atomic<bool> complete = false;
  std::mutex mutex;
  std::optional<std::filesystem::path> selection;
  std::string error;
};

void SDLCALL fileSelected(void *userdata, const char *const *filelist, int) {
  auto &result = *static_cast<DialogResult *>(userdata);
  {
    const std::lock_guard lock(result.mutex);
    if (filelist == nullptr) {
      result.error = SDL_GetError();
    } else if (filelist[0] != nullptr) {
      result.selection = std::filesystem::path(filelist[0]);
    }
  }
  result.complete.store(true, std::memory_order_release);
}

void showError(const std::string &message) {
  lucent::error("crashbash-setup", "{}", message);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Crash Bash Setup", message.c_str(), nullptr);
}

bool chooseBrowse() {
  constexpr SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Browse…"},
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
  };
  const SDL_MessageBoxData dialog{
      .flags = SDL_MESSAGEBOX_INFORMATION,
      .window = nullptr,
      .title = "Crash Bash Setup",
      .message = "Crash Bash needs your North American (SCUS-94570) disc image.\n\n"
                 "The original game is not included. Choose a .chd file and it will be verified "
                 "before any previous selection is replaced. A .zip containing one .chd is also "
                 "supported.",
      .numbuttons = 2,
      .buttons = buttons,
      .colorScheme = nullptr,
  };
  int selected = 0;
  if (!SDL_ShowMessageBox(&dialog, &selected)) {
    showError(std::string("Cannot open the setup screen: ") + SDL_GetError());
    return false;
  }
  return selected == 1;
}

std::optional<std::filesystem::path> chooseMedia() {
  static constexpr SDL_DialogFileFilter filters[] = {
      {"Crash Bash disc image or ZIP", "chd;zip"},
      {"All files", "*"},
  };
  DialogResult result;
  SDL_ShowOpenFileDialog(fileSelected, &result, nullptr, filters, 2, nullptr, false);
  while (!result.complete.load(std::memory_order_acquire)) {
    SDL_PumpEvents();
    SDL_Delay(10);
  }
  const std::lock_guard lock(result.mutex);
  if (!result.error.empty()) {
    showError("The file picker failed: " + result.error);
  }
  return result.selection;
}

bool configureLaunchEnvironment(const UserPaths &paths, const AppLayout &layout, const std::filesystem::path &disc) {
  const auto set = [](const char *name, const std::filesystem::path &value) {
#ifdef _WIN32
    return _putenv_s(name, value.string().c_str()) == 0;
#else
    return ::setenv(name, value.c_str(), 1) == 0;
#endif
  };
  return set("PSXPORT_CRASHBASH_DISC", disc) && set("PSXPORT_SETTINGS", paths.settings) &&
         set("PSXPORT_CARD", paths.memoryCard) && set("PSXPORT_ASSET_DIR", layout.shareDirectory) &&
#ifdef _WIN32
         _putenv_s("PSXPORT_VK_WINDOW", "1") == 0;
#else
         ::setenv("PSXPORT_VK_WINDOW", "1", 1) == 0;
#endif
}

int launchGame(const UserPaths &paths, const AppLayout &layout, const std::filesystem::path &disc) {
  if (!configureLaunchEnvironment(paths, layout, disc)) {
    showError("Cannot configure the Crash Bash user-data paths.");
    return 2;
  }
  std::error_code filesystemError;
  std::filesystem::current_path(paths.dataDirectory, filesystemError);
  if (filesystemError) {
    showError("Cannot enter the Crash Bash user-data directory: " + filesystemError.message());
    return 2;
  }
  SDL_Quit();
#ifdef _WIN32
  showError("The AppImage launcher is supported only on Linux.");
  return 2;
#else
  ::execl(layout.gameBinary.c_str(), layout.gameBinary.c_str(), paths.executable.c_str(), static_cast<char *>(nullptr));
  showError("Cannot launch the packaged game binary.");
  return 2;
#endif
}

int selftest(const AppLayout &layout, const MediaIdentity &identity) {
  std::size_t checks = 0;
  checks += std::filesystem::is_regular_file(layout.gameBinary) ? 1 : 0;
  checks += std::filesystem::is_regular_file(layout.discdump) ? 1 : 0;
  checks += std::filesystem::is_regular_file(layout.sha256sum) ? 1 : 0;
  checks += std::filesystem::is_directory(layout.assets / "rml") ? 1 : 0;
  checks += identity.executableName == "SCUS_945.70" ? 1 : 0;
  checks += identity.executableSize > 0 && identity.dataSize > identity.executableSize ? 1 : 0;
  if (checks != 6) {
    lucent::error("crashbash-setup", "AppImage launcher selftest: {}/6 checks passed", checks);
    return 1;
  }
  lucent::info("crashbash-setup", "AppImage launcher selftest: 6/6 checks passed");
  return 0;
}

} // namespace
} // namespace crashbash::appimage

int main(int argc, char **argv) {
  using namespace crashbash::appimage;
  std::string error;
  const auto layout = resolveAppLayout(error);
  if (!layout.has_value()) {
    lucent::error("crashbash-setup", "{}", error);
    return 2;
  }
  const auto identity = loadMediaIdentity(layout->identity, error);
  if (!identity.has_value()) {
    lucent::error("crashbash-setup", "{}", error);
    return 2;
  }
  if (argc == 2 && std::string_view(argv[1]) == "--selftest") {
    return selftest(*layout, *identity);
  }

  const auto paths = resolveUserPaths(error);
  if (!paths.has_value()) {
    showError(error);
    return 2;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--provision") {
    const std::filesystem::path selected = std::filesystem::absolute(argv[2]);
    if (!provisionSelection(selected, *layout, *paths, *identity, error)) {
      lucent::error("crashbash-setup", "{}", error);
      return 1;
    }
    lucent::info("crashbash-setup", "verified and installed {}", selected.string());
    return 0;
  }

  const bool forceSetup = argc == 2 && std::string_view(argv[1]) == "--setup";
  auto disc = forceSetup ? std::nullopt : loadConfiguredDisc(*paths, error);
  if (disc.has_value() && std::filesystem::is_regular_file(*disc) &&
      executableMatches(paths->executable, *layout, *identity, error)) {
    return launchGame(*paths, *layout, *disc);
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    lucent::error("crashbash-setup", "cannot initialise the desktop UI: {}", SDL_GetError());
    return 2;
  }
  while (chooseBrowse()) {
    const auto selected = chooseMedia();
    if (!selected.has_value()) {
      continue;
    }
    error.clear();
    if (!provisionSelection(std::filesystem::absolute(*selected), *layout, *paths, *identity, error)) {
      showError(error);
      continue;
    }
    return launchGame(*paths, *layout, std::filesystem::absolute(*selected));
  }
  SDL_Quit();
  return 0;
}
