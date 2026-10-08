#include "image_identity_state.h"

#include "state_blob.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>
#include <span>

namespace crashbash::runtime {
namespace {

constexpr std::uint32_t kMainRamBytes = 0x200000u;
constexpr std::uint32_t kMaxRecords = 16u;
constexpr std::uint32_t kMaxResidentRanges = 64u;
constexpr std::uint32_t kMaxNameBytes = 64u;

bool knownImage(std::uint32_t value) {
  return value <= static_cast<std::uint32_t>(GuestImage::Dat28382);
}

bool ramRange(GuestAddressRange range) {
  return range.begin < range.end && range.end <= kMainRamBytes;
}

void writeRange(psx::state::BlobWriter &out, GuestAddressRange range) {
  out.u32(range.begin);
  out.u32(range.end);
}

GuestAddressRange readRange(psx::state::BlobReader &in) {
  const std::uint32_t begin = in.u32();
  const std::uint32_t end = in.u32();
  return {begin, end};
}

bool validRecord(const BoundImageRecord &record, std::string &error) {
  if (record.name.empty() || !ramRange(record.range)) {
    error = "image '" + record.name + "' has no name or a published range outside main RAM";
    return false;
  }
  if (record.residentRanges.empty()) {
    error = "image '" + record.name + "' carries no surviving range";
    return false;
  }
  std::uint32_t floor = record.range.begin;
  for (const GuestAddressRange surviving : record.residentRanges) {
    if (surviving.begin < floor || surviving.begin >= surviving.end || surviving.end > record.range.end) {
      error = "image '" + record.name + "' carries a surviving range that is empty, out of order, overlapping, " +
              "or outside its published range";
      return false;
    }
    floor = surviving.end;
  }
  return true;
}

} // namespace

void writeBoundImages(psx::state::BlobWriter &out, const std::vector<BoundImageRecord> &records) {
  out.u32(static_cast<std::uint32_t>(records.size()));
  for (const BoundImageRecord &record : records) {
    out.u32(static_cast<std::uint32_t>(record.image));
    out.blob(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(record.name.data()), record.name.size()));
    out.u64(record.contentIdentity);
    writeRange(out, record.range);
    out.u32(static_cast<std::uint32_t>(record.residentRanges.size()));
    for (const GuestAddressRange surviving : record.residentRanges) {
      writeRange(out, surviving);
    }
  }
}

std::optional<std::vector<BoundImageRecord>> readBoundImages(psx::state::BlobReader &in, std::string &error) {
  const std::uint32_t count = in.u32();
  if (!in.ok() || count == 0u || count > kMaxRecords) {
    error = "the image-identity section names " + std::to_string(count) + " images";
    return std::nullopt;
  }
  std::vector<BoundImageRecord> records;
  records.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    BoundImageRecord record;
    const std::uint32_t image = in.u32();
    std::vector<std::uint8_t> name;
    const bool nameRead = in.blob(name);
    record.contentIdentity = in.u64();
    record.range = readRange(in);
    const std::uint32_t surviving = in.u32();
    if (!in.ok() || !nameRead || name.size() > kMaxNameBytes || surviving > kMaxResidentRanges) {
      error = "the image-identity section is truncated or oversized at record " + std::to_string(index);
      return std::nullopt;
    }
    if (!knownImage(image)) {
      error = "the image-identity section names logical image " + std::to_string(image) + ", which this title lacks";
      return std::nullopt;
    }
    record.image = static_cast<GuestImage>(image);
    record.name.assign(name.begin(), name.end());
    for (std::uint32_t part = 0; part < surviving; ++part) {
      record.residentRanges.push_back(readRange(in));
    }
    if (!in.ok()) {
      error = "the image-identity section is truncated in record " + std::to_string(index);
      return std::nullopt;
    }
    if (!validRecord(record, error)) {
      return std::nullopt;
    }
    if (std::any_of(records.begin(), records.end(), [&record](const BoundImageRecord &earlier) {
          return earlier.image == record.image;
        })) {
      error = "the image-identity section binds image '" + record.name + "' twice";
      return std::nullopt;
    }
    records.push_back(std::move(record));
  }
  if (std::none_of(records.begin(), records.end(), [](const BoundImageRecord &record) {
        return record.image == GuestImage::Resident;
      })) {
    error = "the image-identity section carries no resident executable";
    return std::nullopt;
  }
  return records;
}

bool ImageIdentityState::save(psx::state::BlobWriter &out, std::string &error) const {
  if (!execution_.ownsEveryActiveResidency()) {
    error = "an active image residency is not one of this title's bindings, so the image identity is not "
            "the title's to record";
    return false;
  }
  writeBoundImages(out, execution_.boundImages());
  return true;
}

bool ImageIdentityState::load(psx::state::BlobReader &in, std::string &error) {
  staged_.reset();
  if (!execution_.ownsEveryActiveResidency()) {
    error = "an active image residency is not one of this title's bindings, so restoring the title's would "
            "leave it describing bytes the state replaces";
    return false;
  }
  auto records = readBoundImages(in, error);
  if (!records) {
    return false;
  }
  staged_ = std::move(records);
  return true;
}

void ImageIdentityState::restored(Core &) {
  if (!staged_) {
    lucent::error("crashbash-module", "a state was restored without the image identities its load staged");
    std::abort();
  }
  const std::vector<BoundImageRecord> records = std::move(*staged_);
  staged_.reset();
  execution_.restoreBoundImages(records);
  for (const BoundImageRecord &record : records) {
    lucent::info("crashbash-module",
                 "restored {} identity over {} surviving range(s) of 0x{:06X}..0x{:06X}",
                 record.name,
                 record.residentRanges.size(),
                 record.range.begin,
                 record.range.end);
  }
}

} // namespace crashbash::runtime
