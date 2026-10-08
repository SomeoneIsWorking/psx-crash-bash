#pragma once

#include "native_dispatch.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Core;
namespace psx::state {
class NativeStatePort;
}

namespace crashbash::runtime {

class ImageIdentityState;

// Crash Bash reuses guest address ranges for unrelated modules, so image identity is part of every
// override key.
enum class GuestImage {
  Resident,
  Boot,
  Menu,
  Dat28136,
  Dat22510,
  // Other occupants of the reused 0x800B32B4 nested slot.
  Dat28272,
  Dat28241,
  Dat28382,
};

using NativeOverride = void (*)(Core *);

// One authenticated binding as a save state records it: image, published residency, and the
// physical ranges no later write has retired.
struct BoundImageRecord {
  GuestImage image = GuestImage::Resident;
  std::string name;
  std::uint64_t contentIdentity = 0;
  GuestAddressRange range;
  std::vector<GuestAddressRange> residentRanges;
};

// Per-Core title context. The loader authenticates and activates the ImageCatalog residency
// before binding here, and unbinds before unloading.
class GuestExecution final {
public:
  explicit GuestExecution(Core &core);
  ~GuestExecution();
  GuestExecution(const GuestExecution &) = delete;
  GuestExecution &operator=(const GuestExecution &) = delete;
  GuestExecution(GuestExecution &&) = delete;
  GuestExecution &operator=(GuestExecution &&) = delete;

  std::size_t registeredOverrideCount() const {
    return registrations_.size();
  }

  // A producer override opens its emission scope around every call (psxport presentation.md, Frame model).
  void registerOverride(GuestImage image,
                        std::uint32_t address,
                        std::string_view name,
                        NativeOverride function,
                        std::optional<psx::present::Producer> producer = std::nullopt);
  void bindAuthenticatedImage(GuestImage image, psx::cpu::ImageIdentity identity, GuestAddressRange range);
  void unbindImage(GuestImage image);
  // Removes only the overwritten bytes from a generation and retires native keys at overwritten entries.
  void retireImagesOverlapping(GuestAddressRange physicalRange);
  std::optional<psx::cpu::NativeKey> activeKey(GuestImage image, std::uint32_t address) const;

  // Every binding with its surviving residency, in publication order.
  std::vector<BoundImageRecord> boundImages() const;
  // True when every active residency on the catalog is one of this context's bindings.
  bool ownsEveryActiveResidency() const;
  // Replaces every binding with `records` from `boundImages`: retires the current residencies, then
  // publishes each record as a fresh generation and retires what it says no longer survives.
  void restoreBoundImages(const std::vector<BoundImageRecord> &records);

  // This Core's native state in a whole-machine save state (image_identity_state.h).
  psx::state::NativeStatePort &statePort();

  // One bounded host turn of the original body; BudgetExhausted is an ordinary result handled by
  // runGuestCallToReturn.
  psx::cpu::ExecutionResult original(GuestImage image, std::uint32_t address, psx::cpu::ExecutionBudget budget);

private:
  struct Registration {
    GuestImage image;
    std::uint32_t address;
    std::string name;
    NativeOverride function;
    std::optional<psx::present::Producer> producer;
  };
  struct Binding {
    GuestImage image;
    psx::cpu::ImageIdentity identity;
    GuestAddressRange range;
  };
  void removeRegistrations(const Binding &binding);
  // Returns false when nothing of the residency survives; the caller then erases the binding.
  bool retireBindingRange(const Binding &binding, GuestAddressRange physicalRange);

  Core &core_;
  std::vector<Registration> registrations_;
  std::vector<Binding> bindings_;
  std::unique_ptr<ImageIdentityState> statePort_;
};

void registerNativeOverride(Core &core,
                            GuestImage image,
                            std::uint32_t address,
                            std::string_view name,
                            NativeOverride function,
                            std::optional<psx::present::Producer> producer = std::nullopt);
void retireAuthenticatedImagesForWrite(Core &core, GuestAddressRange physicalRange);

// Callers set up the guest ABI state (including r31 and instruction timing) first.
void dispatchGuest(Core &core, std::uint32_t address);
psx::cpu::ExecutionResult dispatchGuestSlice(Core &core, std::uint32_t address, psx::cpu::ExecutionBudget budget);

// Turn cap per guest or original call. The framework default (8) is below the worst case here, the
// full-frame 15-bit channel swap (MENU 0x800B5244, BOOT 0x8008E5BC).
inline constexpr std::uint32_t kTitleCallTurnCap = 12u;

// Runs a guest or original call to its return address over as many bounded turns as it needs. A
// refused call is fatal: the reason is reported and the process aborts.
// `returnPc` is the caller's return address as the first turn saw it, so a resume does not adopt a
// nested `$ra`. `original` names the override key the first turn suppressed. Returns the guest's r[2].
std::uint32_t runGuestCallToReturn(Core &core,
                                   std::uint32_t entry,
                                   std::uint32_t returnPc,
                                   std::string_view owner,
                                   const std::optional<psx::cpu::NativeKey> &original);

// Prints the title's turn cap beside psxport's guest-call census line.
void reportGuestCallTurnCap(Core &core, std::string_view why);

// Runs the guest body of the active override with only that override suppressed, to the return address.
void callOriginal(Core &core, GuestImage image, std::uint32_t address);

// Whether `address` is code of the image's current authenticated generation.
bool imageHolds(Core &core, GuestImage image, std::uint32_t address);

} // namespace crashbash::runtime
