#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace crashbash::appimage {

struct UserPaths {
  std::filesystem::path configDirectory;
  std::filesystem::path dataDirectory;
  std::filesystem::path stateDirectory;
  std::filesystem::path discConfiguration;
  std::filesystem::path executable;
  std::filesystem::path settings;
  std::filesystem::path memoryCard;
};

std::optional<UserPaths> resolveUserPaths(std::string &error);
std::optional<std::filesystem::path> loadConfiguredDisc(const UserPaths &paths, std::string &error);
bool persistConfiguredDisc(const UserPaths &paths, const std::filesystem::path &disc, std::string &error);

} // namespace crashbash::appimage
