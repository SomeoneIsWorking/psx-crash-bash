#pragma once

#include "guest_execution.h"
#include "machine_state.h"

#include <optional>
#include <string>
#include <vector>

namespace crashbash::runtime {

// Save-state section holding the authenticated image identities. A restore replaces RAM wholesale and
// the identities cannot be re-derived from restored bytes (live modules no longer hash to their digest).
// `load` stages and validates; `restored` publishes fresh generations once RAM is in place.
class ImageIdentityState final : public psx::state::NativeStatePort {
public:
  explicit ImageIdentityState(GuestExecution &execution) : execution_(execution) {}

  [[nodiscard]] const char *sectionName() const override {
    return "crashbash.image-identity";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 1;
  }
  bool save(psx::state::BlobWriter &out, std::string &error) const override;
  bool load(psx::state::BlobReader &in, std::string &error) override;
  void restored(Core &core) override;

private:
  GuestExecution &execution_;
  std::optional<std::vector<BoundImageRecord>> staged_;
};

// `readBoundImages` refuses, via `error`, anything `writeBoundImages` could not have produced.
void writeBoundImages(psx::state::BlobWriter &out, const std::vector<BoundImageRecord> &records);
std::optional<std::vector<BoundImageRecord>> readBoundImages(psx::state::BlobReader &in, std::string &error);

} // namespace crashbash::runtime
