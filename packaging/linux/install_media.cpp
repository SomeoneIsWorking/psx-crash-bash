#include "install_media.h"

#include <lucent/content.h>
#include <lucent/zip.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fstream>
#include <map>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace crashbash::appimage {
namespace {

constexpr std::size_t kDigestBytes = 32;

class SetupLock {
public:
  explicit SetupLock(const std::filesystem::path &path) {
#ifndef _WIN32
    descriptor_ = ::open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if (descriptor_ >= 0 && ::flock(descriptor_, LOCK_EX | LOCK_NB) != 0) {
      ::close(descriptor_);
      descriptor_ = -1;
    }
#else
    (void)path;
    descriptor_ = 1;
#endif
  }

  SetupLock(const SetupLock &) = delete;
  SetupLock &operator=(const SetupLock &) = delete;

  ~SetupLock() {
#ifndef _WIN32
    if (descriptor_ >= 0) {
      ::flock(descriptor_, LOCK_UN);
      ::close(descriptor_);
    }
#endif
  }

  bool owns() const {
    return descriptor_ >= 0;
  }

private:
  int descriptor_ = -1;
};

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

bool isLowerHexDigest(const std::string &value) {
  return value.size() == kDigestBytes * 2 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return std::isdigit(character) != 0 || (character >= 'a' && character <= 'f');
         });
}

std::optional<std::uintmax_t> parseSize(const std::string &value) {
  std::uintmax_t result = 0;
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
  if (error != std::errc{} || end != value.data() + value.size() || result == 0) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::string>
sha256(const std::filesystem::path &tool, const std::filesystem::path &path, std::string &error) {
#ifdef _WIN32
  (void)tool;
  (void)path;
  error = "AppImage media validation is supported only on Linux";
  return std::nullopt;
#else
  int output[2]{};
  if (::pipe2(output, O_CLOEXEC) != 0) {
    error = "cannot create SHA-256 output pipe: " + std::string(std::strerror(errno));
    return std::nullopt;
  }
  const pid_t child = ::fork();
  if (child < 0) {
    ::close(output[0]);
    ::close(output[1]);
    error = "cannot start SHA-256 verifier: " + std::string(std::strerror(errno));
    return std::nullopt;
  }
  if (child == 0) {
    ::close(output[0]);
    ::dup2(output[1], STDOUT_FILENO);
    ::close(output[1]);
    ::execl(tool.c_str(), tool.c_str(), "--", path.c_str(), static_cast<char *>(nullptr));
    _exit(127);
  }
  ::close(output[1]);
  std::string rendered;
  std::array<char, 256> buffer{};
  for (;;) {
    const ssize_t count = ::read(output[0], buffer.data(), buffer.size());
    if (count == 0) {
      break;
    }
    if (count < 0) {
      ::close(output[0]);
      ::waitpid(child, nullptr, 0);
      error = "cannot read SHA-256 output: " + std::string(std::strerror(errno));
      return std::nullopt;
    }
    rendered.append(buffer.data(), static_cast<std::size_t>(count));
  }
  ::close(output[0]);
  int status = 0;
  if (::waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    error = "cannot hash " + path.string();
    return std::nullopt;
  }
  std::string digest = lowercase(rendered.substr(0, rendered.find_first_of(" \t\r\n")));
  if (!isLowerHexDigest(digest)) {
    error = "SHA-256 verifier returned malformed output for " + path.string();
    return std::nullopt;
  }
  return digest;
#endif
}

bool matchesFile(const std::filesystem::path &path,
                 const std::filesystem::path &sha256sum,
                 std::uintmax_t expectedSize,
                 const std::string &expectedDigest,
                 std::string &error) {
  std::error_code filesystemError;
  if (!std::filesystem::is_regular_file(path, filesystemError)) {
    error = path.string() + " is not a regular file";
    return false;
  }
  const std::uintmax_t actualSize = std::filesystem::file_size(path, filesystemError);
  if (filesystemError || actualSize != expectedSize) {
    error = path.string() + " has " + std::to_string(actualSize) + " bytes; expected " + std::to_string(expectedSize);
    return false;
  }
  const auto actualDigest = sha256(sha256sum, path, error);
  if (!actualDigest.has_value() || *actualDigest != expectedDigest) {
    if (actualDigest.has_value()) {
      error = path.string() + " has SHA-256 " + *actualDigest + "; expected " + expectedDigest;
    }
    return false;
  }
  return true;
}

bool runDiscdump(const AppLayout &layout,
                 const std::string &discPath,
                 const std::filesystem::path &disc,
                 const std::filesystem::path &output,
                 const std::filesystem::path &log,
                 std::string &error) {
#ifdef _WIN32
  (void)layout;
  (void)discPath;
  (void)disc;
  (void)output;
  (void)log;
  error = "AppImage setup is supported only on Linux";
  return false;
#else
  const pid_t child = ::fork();
  if (child < 0) {
    error = "cannot start disc extractor: " + std::string(std::strerror(errno));
    return false;
  }
  if (child == 0) {
    const int logDescriptor = ::open(log.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (logDescriptor >= 0) {
      ::dup2(logDescriptor, STDOUT_FILENO);
      ::dup2(logDescriptor, STDERR_FILENO);
      ::close(logDescriptor);
    }
    ::execl(layout.discdump.c_str(),
            layout.discdump.c_str(),
            "get",
            discPath.c_str(),
            disc.c_str(),
            output.c_str(),
            static_cast<char *>(nullptr));
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) < 0) {
    error = "cannot wait for disc extractor: " + std::string(std::strerror(errno));
    return false;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::ifstream logInput(log);
    std::string detail((std::istreambuf_iterator<char>(logInput)), std::istreambuf_iterator<char>());
    if (detail.size() > 1200) {
      detail = detail.substr(detail.size() - 1200);
    }
    error = "the selected disc could not provide " + discPath;
    if (!detail.empty()) {
      error += ":\n" + detail;
    }
    return false;
  }
  return true;
#endif
}

std::string normalisedBootTarget(const std::filesystem::path &systemCnf) {
  std::ifstream input(systemCnf, std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  text = lowercase(text);
  const std::size_t boot = text.find("boot");
  const std::size_t equals = boot == std::string::npos ? std::string::npos : text.find('=', boot + 4);
  if (equals == std::string::npos) {
    return {};
  }
  const std::size_t end = text.find_first_of("\r\n", equals + 1);
  std::string value = text.substr(equals + 1, end - equals - 1);
  value.erase(std::remove_if(value.begin(),
                             value.end(),
                             [](unsigned char character) {
                               return std::isspace(character) != 0;
                             }),
              value.end());
  constexpr std::string_view prefix = "cdrom:";
  if (value.starts_with(prefix)) {
    value.erase(0, prefix.size());
  }
  while (!value.empty() && (value.front() == '\\' || value.front() == '/')) {
    value.erase(value.begin());
  }
  if (value.ends_with(";1")) {
    value.resize(value.size() - 2);
  }
  std::replace(value.begin(), value.end(), '\\', '/');
  return value;
}

bool publishExecutable(const std::filesystem::path &staged,
                       const std::filesystem::path &destination,
                       std::string &error) {
  const std::filesystem::path pending = destination.parent_path() / ".SCUS_945.70.pending";
  std::error_code filesystemError;
  std::filesystem::copy_file(staged, pending, std::filesystem::copy_options::overwrite_existing, filesystemError);
  if (filesystemError) {
    error = "cannot stage the verified executable: " + filesystemError.message();
    return false;
  }
  std::filesystem::rename(pending, destination, filesystemError);
  if (filesystemError) {
    error = "cannot publish the verified executable: " + filesystemError.message();
    return false;
  }
  return true;
}

bool isChdEntry(std::string_view name, std::span<const std::uint8_t> content) {
  constexpr std::array<std::uint8_t, 8> kChdMagic{'M', 'C', 'o', 'm', 'p', 'r', 'H', 'D'};
  return lowercase(std::filesystem::path(name).extension().string()) == ".chd" && content.size() >= kChdMagic.size() &&
         std::equal(kChdMagic.begin(), kChdMagic.end(), content.begin());
}

bool provisionArchive(const std::filesystem::path &archive,
                      const AppLayout &layout,
                      const UserPaths &paths,
                      const MediaIdentity &identity,
                      std::string &error) {
  std::error_code filesystemError;
  if (!archive.is_absolute() || !std::filesystem::is_regular_file(archive, filesystemError)) {
    error = "the selected ZIP must be an existing regular file";
    return false;
  }

  const auto digest = lucent::content::sha256_file(archive, error);
  if (!digest.has_value()) {
    return false;
  }
  const std::filesystem::path mediaRoot = paths.dataDirectory / "media";
  std::filesystem::create_directories(mediaRoot, filesystemError);
  if (filesystemError) {
    error = "cannot create the installed-media directory: " + filesystemError.message();
    return false;
  }
  const std::filesystem::path destination = mediaRoot / lucent::content::sha256_hex(*digest);
  std::filesystem::path extractedDisc;
  if (!lucent::zip::extract_unique_install(archive, destination, isChdEntry, extractedDisc, error)) {
    return false;
  }
  if (provisionDisc(extractedDisc, layout, paths, identity, error)) {
    return true;
  }

  std::filesystem::remove_all(destination, filesystemError);
  if (filesystemError) {
    error += "; additionally could not remove the rejected ZIP contents: " + filesystemError.message();
  }
  return false;
}

} // namespace

std::optional<AppLayout> resolveAppLayout(std::string &error) {
#ifdef _WIN32
  error = "AppImage setup is supported only on Linux";
  return std::nullopt;
#else
  std::array<char, 4096> executablePath{};
  const ssize_t length = ::readlink("/proc/self/exe", executablePath.data(), executablePath.size() - 1);
  if (length <= 0 || static_cast<std::size_t>(length) >= executablePath.size() - 1) {
    error = "cannot resolve the AppImage launcher path";
    return std::nullopt;
  }
  executablePath[static_cast<std::size_t>(length)] = '\0';
  const std::filesystem::path executableDirectory = std::filesystem::path(executablePath.data()).parent_path();
  const std::filesystem::path shareDirectory = executableDirectory.parent_path() / "share/crashbash";
  AppLayout layout{
      .executableDirectory = executableDirectory,
      .shareDirectory = shareDirectory,
      .gameBinary = executableDirectory / "crashbash_port",
      .discdump = executableDirectory / "discdump",
      .sha256sum = executableDirectory / "sha256sum",
      .identity = shareDirectory / "media-identity.conf",
      .assets = shareDirectory / "assets",
  };
  for (const auto &[description, path] : std::array{
           std::pair{"game binary", layout.gameBinary},
           std::pair{"disc extractor", layout.discdump},
           std::pair{"SHA-256 verifier", layout.sha256sum},
           std::pair{"media identity", layout.identity},
       }) {
    if (!std::filesystem::is_regular_file(path)) {
      error = std::string("the AppImage is missing its ") + description + " at " + path.string();
      return std::nullopt;
    }
  }
  if (!std::filesystem::is_directory(layout.assets)) {
    error = "the AppImage is missing its runtime assets at " + layout.assets.string();
    return std::nullopt;
  }
  return layout;
#endif
}

std::optional<MediaIdentity> loadMediaIdentity(const std::filesystem::path &path, std::string &error) {
  std::ifstream input(path);
  if (!input) {
    error = "cannot read " + path.string();
    return std::nullopt;
  }
  std::map<std::string, std::string> values;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t separator = line.find('=');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= line.size()) {
      error = "invalid media identity line: " + line;
      return std::nullopt;
    }
    if (!values.emplace(line.substr(0, separator), line.substr(separator + 1)).second) {
      error = "duplicate media identity field: " + line.substr(0, separator);
      return std::nullopt;
    }
  }
  constexpr std::array required{
      "executable_name", "executable_size", "executable_sha256", "data_path", "data_size", "data_sha256"};
  for (const char *field : required) {
    if (!values.contains(field)) {
      error = "media identity is missing " + std::string(field);
      return std::nullopt;
    }
  }
  if (values.size() != required.size()) {
    error = "media identity contains an unknown field";
    return std::nullopt;
  }
  const auto executableSize = parseSize(values["executable_size"]);
  const auto dataSize = parseSize(values["data_size"]);
  const std::string executableDigest = lowercase(values["executable_sha256"]);
  const std::string dataDigest = lowercase(values["data_sha256"]);
  if (!executableSize.has_value() || !dataSize.has_value() || !isLowerHexDigest(executableDigest) ||
      !isLowerHexDigest(dataDigest)) {
    error = "media identity contains an invalid size or SHA-256";
    return std::nullopt;
  }
  return MediaIdentity{
      .executableName = values["executable_name"],
      .executableSize = *executableSize,
      .executableSha256 = executableDigest,
      .dataPath = values["data_path"],
      .dataSize = *dataSize,
      .dataSha256 = dataDigest,
  };
}

bool executableMatches(const std::filesystem::path &path,
                       const AppLayout &layout,
                       const MediaIdentity &identity,
                       std::string &error) {
  return matchesFile(path, layout.sha256sum, identity.executableSize, identity.executableSha256, error);
}

bool provisionDisc(const std::filesystem::path &disc,
                   const AppLayout &layout,
                   const UserPaths &paths,
                   const MediaIdentity &identity,
                   std::string &error) {
  std::error_code filesystemError;
  if (!disc.is_absolute() || !std::filesystem::is_regular_file(disc, filesystemError)) {
    error = "the selected disc must be an existing regular file";
    return false;
  }
  if (lowercase(disc.extension().string()) != ".chd") {
    error = "select a .chd image of the North American Crash Bash disc";
    return false;
  }

  SetupLock lock(paths.dataDirectory / ".setup.lock");
  if (!lock.owns()) {
    error = "another Crash Bash setup is already running";
    return false;
  }

  const std::filesystem::path staging = paths.dataDirectory / ".setup-staging";
  std::filesystem::remove_all(staging, filesystemError);
  if (filesystemError) {
    error = "cannot clear the prior setup staging directory: " + filesystemError.message();
    return false;
  }
  std::filesystem::create_directories(staging, filesystemError);
  if (filesystemError) {
    error = "cannot create setup staging: " + filesystemError.message();
    return false;
  }

  const std::filesystem::path log = staging / "provision.log";
  const bool extracted = runDiscdump(layout, "SYSTEM.CNF", disc, staging, log, error) &&
                         runDiscdump(layout, identity.executableName, disc, staging, log, error) &&
                         runDiscdump(layout, identity.dataPath, disc, staging, log, error);
  if (!extracted) {
    std::filesystem::remove_all(staging, filesystemError);
    return false;
  }

  const std::filesystem::path executable = staging / identity.executableName;
  const std::filesystem::path data = staging / std::filesystem::path(identity.dataPath).filename();
  if (normalisedBootTarget(staging / "SYSTEM.CNF") != lowercase(identity.executableName)) {
    error = "SYSTEM.CNF does not boot " + identity.executableName;
  } else if (!matchesFile(executable, layout.sha256sum, identity.executableSize, identity.executableSha256, error)) {
    // `error` names the mismatched executable fact.
  } else if (!matchesFile(data, layout.sha256sum, identity.dataSize, identity.dataSha256, error)) {
    // The complete data file is checked before the previous selection is replaced.
  } else if (!publishExecutable(executable, paths.executable, error)) {
    // `error` names the failed atomic publication.
  } else if (!persistConfiguredDisc(paths, disc, error)) {
    // A failed config publication preserves the previous selected path.
  } else {
    std::filesystem::remove_all(staging, filesystemError);
    return true;
  }

  std::filesystem::remove_all(staging, filesystemError);
  return false;
}

bool provisionSelection(const std::filesystem::path &selection,
                        const AppLayout &layout,
                        const UserPaths &paths,
                        const MediaIdentity &identity,
                        std::string &error) {
  const std::string extension = lowercase(selection.extension().string());
  if (extension == ".chd") {
    return provisionDisc(selection, layout, paths, identity, error);
  }
  if (extension == ".zip") {
    return provisionArchive(selection, layout, paths, identity, error);
  }
  error = "select a .chd image or .zip archive of the North American Crash Bash disc";
  return false;
}

} // namespace crashbash::appimage
