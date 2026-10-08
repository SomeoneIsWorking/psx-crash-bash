#pragma once

#include "user_paths.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace crashbash::appimage {

struct MediaIdentity {
  std::string executableName;
  std::uintmax_t executableSize = 0;
  std::string executableSha256;
  std::string dataPath;
  std::uintmax_t dataSize = 0;
  std::string dataSha256;
};

struct AppLayout {
  std::filesystem::path executableDirectory;
  std::filesystem::path shareDirectory;
  std::filesystem::path gameBinary;
  std::filesystem::path discdump;
  std::filesystem::path sha256sum;
  std::filesystem::path identity;
  std::filesystem::path assets;
};

std::optional<AppLayout> resolveAppLayout(std::string &error);
std::optional<MediaIdentity> loadMediaIdentity(const std::filesystem::path &path, std::string &error);
bool executableMatches(const std::filesystem::path &path,
                       const AppLayout &layout,
                       const MediaIdentity &identity,
                       std::string &error);
bool provisionDisc(const std::filesystem::path &disc,
                   const AppLayout &layout,
                   const UserPaths &paths,
                   const MediaIdentity &identity,
                   std::string &error);
bool provisionSelection(const std::filesystem::path &selection,
                        const AppLayout &layout,
                        const UserPaths &paths,
                        const MediaIdentity &identity,
                        std::string &error);

} // namespace crashbash::appimage
