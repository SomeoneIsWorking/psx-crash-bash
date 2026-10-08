#include "user_paths.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace crashbash::appimage {
namespace {

constexpr const char *kApplicationDirectory = "crashbash-port";

std::optional<std::filesystem::path> absoluteEnvironmentPath(const char *name) {
  const char *value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  std::filesystem::path path(value);
  if (!path.is_absolute()) {
    return std::nullopt;
  }
  return path;
}

bool ensureDirectory(const std::filesystem::path &path, std::string &error) {
  std::error_code filesystemError;
  std::filesystem::create_directories(path, filesystemError);
  if (filesystemError) {
    error = "cannot create " + path.string() + ": " + filesystemError.message();
    return false;
  }
  return true;
}

} // namespace

std::optional<UserPaths> resolveUserPaths(std::string &error) {
  const auto home = absoluteEnvironmentPath("HOME");
  if (!home.has_value()) {
    error = "HOME must name an absolute directory";
    return std::nullopt;
  }

  const std::filesystem::path configBase = absoluteEnvironmentPath("XDG_CONFIG_HOME").value_or(*home / ".config");
  const std::filesystem::path dataBase = absoluteEnvironmentPath("XDG_DATA_HOME").value_or(*home / ".local/share");
  const std::filesystem::path stateBase = absoluteEnvironmentPath("XDG_STATE_HOME").value_or(*home / ".local/state");
  UserPaths paths{};
  paths.configDirectory = configBase / kApplicationDirectory;
  paths.dataDirectory = dataBase / kApplicationDirectory;
  paths.stateDirectory = stateBase / kApplicationDirectory;
  paths.discConfiguration = paths.configDirectory / "disc-path";
  paths.executable = paths.dataDirectory / "SCUS_945.70";
  paths.settings = paths.configDirectory / "settings.ini";
  paths.memoryCard = paths.dataDirectory / "card.mcr";

  if (!ensureDirectory(paths.configDirectory, error) || !ensureDirectory(paths.dataDirectory, error) ||
      !ensureDirectory(paths.stateDirectory, error)) {
    return std::nullopt;
  }
  return paths;
}

std::optional<std::filesystem::path> loadConfiguredDisc(const UserPaths &paths, std::string &error) {
  std::ifstream input(paths.discConfiguration, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::string line;
  std::getline(input, line);
  if (!input.eof() && !input.good()) {
    error = "cannot read " + paths.discConfiguration.string();
    return std::nullopt;
  }
  if (line.empty() || line.find('\0') != std::string::npos || line.find('\r') != std::string::npos) {
    error = "the saved disc path is invalid";
    return std::nullopt;
  }
  std::filesystem::path disc(line);
  if (!disc.is_absolute()) {
    error = "the saved disc path is not absolute";
    return std::nullopt;
  }
  return disc;
}

bool persistConfiguredDisc(const UserPaths &paths, const std::filesystem::path &disc, std::string &error) {
  const std::string encoded = disc.string();
  if (!disc.is_absolute() || encoded.empty() || encoded.find('\n') != std::string::npos ||
      encoded.find('\r') != std::string::npos || encoded.find('\0') != std::string::npos) {
    error = "the selected disc path cannot be persisted safely";
    return false;
  }

  const std::filesystem::path pending = paths.configDirectory / ".disc-path.pending";
#ifndef _WIN32
  const int descriptor = ::open(pending.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (descriptor < 0) {
    error = "cannot create " + pending.string();
    return false;
  }
  const std::string payload = encoded + "\n";
  std::size_t written = 0;
  while (written < payload.size()) {
    const ssize_t result = ::write(descriptor, payload.data() + written, payload.size() - written);
    if (result <= 0) {
      ::close(descriptor);
      error = "cannot write " + pending.string();
      return false;
    }
    written += static_cast<std::size_t>(result);
  }
  if (::fsync(descriptor) != 0 || ::close(descriptor) != 0) {
    error = "cannot flush " + pending.string();
    return false;
  }
#else
  std::ofstream output(pending, std::ios::binary | std::ios::trunc);
  output << encoded << '\n';
  output.flush();
  if (!output) {
    error = "cannot write " + pending.string();
    return false;
  }
#endif

  std::error_code filesystemError;
  std::filesystem::rename(pending, paths.discConfiguration, filesystemError);
  if (filesystemError) {
    error = "cannot publish " + paths.discConfiguration.string() + ": " + filesystemError.message();
    return false;
  }
  return true;
}

} // namespace crashbash::appimage
