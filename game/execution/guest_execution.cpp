#include "guest_execution.h"

#include "core.h"
#include "execution_control.h"
#include "image_identity.h"
#include "image_identity_state.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"
#include "resumable_guest_call.h"

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>
#include <stdexcept>

namespace crashbash::runtime {
namespace {

GuestExecution &execution(Core &core) {
  if (!core.gameCtx) {
    lucent::error("crashbash-runtime", "guest execution requires the title's per-Core context");
    std::abort();
  }
  return *static_cast<GuestExecution *>(core.gameCtx);
}

double displayFields(const Core &core, std::uint64_t cycles) {
  return static_cast<double>(cycles) / static_cast<double>(psx::cpu::ExecutionBudget::currentTurn(core).cycles);
}

const char *imageName(GuestImage image) {
  switch (image) {
  case GuestImage::Resident:
    return "resident";
  case GuestImage::Boot:
    return "boot";
  case GuestImage::Menu:
    return "menu";
  case GuestImage::Dat28136:
    return "dat28136";
  case GuestImage::Dat22510:
    return "dat22510";
  case GuestImage::Dat28272:
    return "dat28272";
  case GuestImage::Dat28241:
    return "dat28241";
  case GuestImage::Dat28382:
    return "dat28382";
  }
  return "unknown";
}

} // namespace

GuestExecution::GuestExecution(Core &core) : core_(core), statePort_(std::make_unique<ImageIdentityState>(*this)) {
  if (core.gameCtx) {
    throw std::logic_error("Crash Bash Core already has a title context");
  }
  core.gameCtx = this;
}

GuestExecution::~GuestExecution() {
  for (const auto &binding : bindings_) {
    removeRegistrations(binding);
  }
  core_.gameCtx = nullptr;
}

void GuestExecution::removeRegistrations(const Binding &binding) {
  for (const auto &registration : registrations_) {
    // Explicit key: the registration may outlive its generation, so resolving the address through
    // the active catalog would name the wrong one.
    if (registration.image == binding.image) {
      core_.nativeDispatcher().remove({binding.identity, registration.address});
    }
  }
}

void GuestExecution::registerOverride(GuestImage image,
                                      std::uint32_t address,
                                      std::string_view name,
                                      NativeOverride function,
                                      std::optional<psx::present::Producer> producer) {
  if (!function || name.empty() || (address & 3u) != 0u) {
    throw std::invalid_argument("Crash Bash native override is incomplete or unaligned");
  }
  if (std::any_of(registrations_.begin(), registrations_.end(), [=](const Registration &entry) {
        return entry.image == image && entry.address == address;
      })) {
    throw std::invalid_argument("Crash Bash native override is already registered");
  }
  const auto binding = std::find_if(bindings_.begin(), bindings_.end(), [image](const Binding &entry) {
    return entry.image == image;
  });
  if (binding != bindings_.end()) {
    if (!binding->range.containsPhysical(address) || core_.currentImageIdentity(address) != binding->identity) {
      throw std::invalid_argument("Crash Bash override does not belong to the active authenticated image");
    }
    if (!psx::cpu::tryInstallNativeOverride(core_, address, name, function, producer)) {
      throw std::logic_error("Crash Bash native override conflicts with an existing runtime owner");
    }
  }
  registrations_.push_back({image, address, std::string(name), function, producer});
}

void GuestExecution::bindAuthenticatedImage(GuestImage image,
                                            psx::cpu::ImageIdentity identity,
                                            GuestAddressRange range) {
  if (core_.currentImageIdentity(range) != identity || identity.id == 0u || identity.generation == 0u) {
    throw std::invalid_argument("Crash Bash image binding requires one complete active authenticated residency");
  }
  for (const auto &binding : bindings_) {
    if (binding.identity == identity) {
      throw std::invalid_argument("Crash Bash image generation is already bound");
    }
  }
  for (const auto &registration : registrations_) {
    if (registration.image != image) {
      continue;
    }
    if (!range.containsPhysical(registration.address) ||
        core_.nativeDispatcher().isInstalled({identity, registration.address})) {
      throw std::invalid_argument("Crash Bash image cannot bind its registered native owners");
    }
  }
  unbindImage(image);
  const Binding binding{image, identity, range};
  bindings_.push_back(binding);
  for (const auto &registration : registrations_) {
    if (registration.image == image &&
        !psx::cpu::tryInstallNativeOverride(
            core_, registration.address, registration.name, registration.function, registration.producer)) {
      lucent::error("crashbash-runtime", "validated image binding failed to install a native owner");
      std::abort();
    }
  }
}

void GuestExecution::unbindImage(GuestImage image) {
  const auto binding = std::find_if(bindings_.begin(), bindings_.end(), [image](const Binding &entry) {
    return entry.image == image;
  });
  if (binding != bindings_.end()) {
    removeRegistrations(*binding);
    bindings_.erase(binding);
  }
}

bool GuestExecution::retireBindingRange(const Binding &binding, GuestAddressRange physicalRange) {
  const auto remaining = core_.imageCatalog().subtractRange(binding.identity, physicalRange);
  if (remaining == 0u) {
    removeRegistrations(binding);
    return false;
  }
  for (const auto &registration : registrations_) {
    // The EXPLICIT key, not psxport's `removeNativeOverride`: `subtractRange` above has already
    // retired the residency these addresses lived in, so resolving the address through the active
    // catalog would find no owner and refuse to remove the very key this loop is retiring.
    if (registration.image == binding.image && physicalRange.containsPhysical(registration.address)) {
      core_.nativeDispatcher().remove({binding.identity, registration.address});
    }
  }
  return true;
}

void GuestExecution::retireImagesOverlapping(GuestAddressRange physicalRange) {
  if (!physicalRange.valid() || physicalRange.end > sizeof(core_.ram)) {
    throw std::invalid_argument("Crash Bash loaded-image retirement requires one physical range");
  }
  for (auto binding = bindings_.begin(); binding != bindings_.end();) {
    if (binding->range.end <= physicalRange.begin || binding->range.begin >= physicalRange.end) {
      ++binding;
      continue;
    }
    if (!retireBindingRange(*binding, physicalRange)) {
      binding = bindings_.erase(binding);
      continue;
    }
    ++binding;
  }
}

std::vector<BoundImageRecord> GuestExecution::boundImages() const {
  std::vector<const Binding *> ordered;
  ordered.reserve(bindings_.size());
  for (const auto &binding : bindings_) {
    ordered.push_back(&binding);
  }
  std::sort(ordered.begin(), ordered.end(), [](const Binding *left, const Binding *right) {
    return left->identity.generation < right->identity.generation;
  });
  std::vector<BoundImageRecord> records;
  records.reserve(ordered.size());
  for (const Binding *binding : ordered) {
    const auto description = core_.imageCatalog().describe(binding->identity);
    if (!description) {
      throw std::logic_error("Crash Bash binding names an image generation its catalog never published");
    }
    records.push_back(
        {binding->image, description->name, description->contentIdentity, binding->range, description->ranges});
  }
  return records;
}

psx::state::NativeStatePort &GuestExecution::statePort() {
  return *statePort_;
}

bool GuestExecution::ownsEveryActiveResidency() const {
  return core_.imageCatalog().activeCount() == bindings_.size();
}

void GuestExecution::restoreBoundImages(const std::vector<BoundImageRecord> &records) {
  for (const auto &binding : bindings_) {
    removeRegistrations(binding);
    core_.imageCatalog().deactivate(binding.identity);
  }
  bindings_.clear();
  for (const BoundImageRecord &record : records) {
    const psx::cpu::ImageIdentity identity =
        core_.imageCatalog().activate(record.name, record.range, record.contentIdentity);
    bindAuthenticatedImage(record.image, identity, record.range);
    // The record's surviving ranges are sorted and disjoint inside its range; every gap between them
    // is a span a later write retired before the state was taken.
    std::uint32_t cursor = record.range.begin;
    std::vector<GuestAddressRange> retired;
    for (const GuestAddressRange surviving : record.residentRanges) {
      if (cursor < surviving.begin) {
        retired.push_back({cursor, surviving.begin});
      }
      cursor = surviving.end;
    }
    if (cursor < record.range.end) {
      retired.push_back({cursor, record.range.end});
    }
    const Binding binding{record.image, identity, record.range};
    for (const GuestAddressRange gap : retired) {
      if (!retireBindingRange(binding, gap)) {
        throw std::logic_error("Crash Bash restored image record had no surviving residency");
      }
    }
  }
}

std::optional<psx::cpu::NativeKey> GuestExecution::activeKey(GuestImage image, std::uint32_t address) const {
  for (const auto &binding : bindings_) {
    if (binding.image == image && binding.range.containsPhysical(address) &&
        core_.currentImageIdentity(address) == binding.identity) {
      return psx::cpu::NativeKey{binding.identity, address};
    }
  }
  return std::nullopt;
}

psx::cpu::ExecutionResult
GuestExecution::original(GuestImage image, std::uint32_t address, psx::cpu::ExecutionBudget budget) {
  const auto key = activeKey(image, address);
  if (!key || !core_.nativeDispatcher().isInstalled(*key)) {
    return {psx::cpu::ExecutionExitReason::Fault,
            address,
            0,
            "Crash Bash original call has no native owner in the current authenticated image generation"};
  }
  return psx::cpu::callOriginal(core_, *key, budget);
}

void registerNativeOverride(Core &core,
                            GuestImage image,
                            std::uint32_t address,
                            std::string_view name,
                            NativeOverride function,
                            std::optional<psx::present::Producer> producer) {
  execution(core).registerOverride(image, address, name, function, producer);
}

void retireAuthenticatedImagesForWrite(Core &core, GuestAddressRange physicalRange) {
  execution(core).retireImagesOverlapping(physicalRange);
}

void dispatchGuest(Core &core, std::uint32_t address) {
  const psx::cpu::ExecutionResult result =
      dispatchGuestSlice(core, address, psx::cpu::ExecutionBudget::currentTurn(core));
  if (!psx::cpu::requireGuestReturn(result, "Crash Bash guest call")) {
    lucent::error("crashbash-guest",
                  "the guest call entered at 0x{:08X} did not return: stopped at 0x{:08X}: {}",
                  address,
                  result.guestPc,
                  result.detail);
    std::abort();
  }
}

psx::cpu::ExecutionResult dispatchGuestSlice(Core &core, std::uint32_t address, psx::cpu::ExecutionBudget budget) {
  return psx::cpu::dispatchGuest(core, address, budget);
}

// A refused guest call leaves no valid state to continue in, so it aborts.
void refuseGuestCall(
    Core &core, std::string_view owner, std::uint32_t entry, std::uint32_t returnPc, const psx::cpu::CallStep &step) {
  lucent::error("crashbash-guest",
                "{}: guest call 0x{:08X} to return address 0x{:08X} refused after {} host turn(s) and "
                "{} cycles ({:.3f} display fields), stopped at 0x{:08X}: {}",
                owner,
                entry,
                returnPc,
                step.turns,
                step.cycles,
                displayFields(core, step.cycles),
                step.guestPc,
                step.detail);
  std::abort();
}

// A resumed original is only suppressed while its image generation is still mapped at the entry;
// otherwise the guest would re-enter the override it is running inside.
bool originalStillMapped(Core &core, std::uint32_t entry, const psx::cpu::NativeKey &original, std::string_view owner) {
  const auto active = core.currentImageIdentity(entry);
  if (active && *active == original.image) {
    return true;
  }
  lucent::error("crashbash-guest",
                "{}: guest call 0x{:08X} outlived its host turn, but the authenticated image generation "
                "behind it is no longer mapped at that address. Its override is no longer suppressed, so "
                "resuming would re-enter it from inside the original body",
                owner,
                entry);
  std::abort();
  return false;
}

void reportGuestCallTurnCap(Core &core, std::string_view why) {
  const std::uint32_t deepest = core.guestCallCensus().deepestTurns();
  if (deepest == 0u) {
    return;
  }
  lucent::info("crashbash-guest",
               "run-end ({}): this title's guest-call cap is {} display field(s) (psxport's default is "
               "{}, which is below this title's measured worst case); the deepest call this run measured "
               "needed {} host turn(s)",
               why,
               kTitleCallTurnCap,
               psx::cpu::kDefaultCallTurns,
               deepest);
}

std::uint32_t runGuestCallToReturn(Core &core,
                                   std::uint32_t entry,
                                   std::uint32_t returnPc,
                                   std::string_view owner,
                                   const std::optional<psx::cpu::NativeKey> &original) {
  psx::cpu::ResumableGuestCall call;
  call.begin(core, owner, entry, returnPc, kTitleCallTurnCap);
  for (;;) {
    if (original && call.turns() > 0u && !originalStillMapped(core, entry, *original, owner)) {
      return 0u;
    }
    const psx::cpu::CallStep step = call.advance(original);
    switch (step.outcome) {
    case psx::cpu::CallOutcome::Returned:
      return step.value;
    case psx::cpu::CallOutcome::Suspended:
      break;
    case psx::cpu::CallOutcome::Refused:
      refuseGuestCall(core, owner, entry, returnPc, step);
      return 0u;
    }
  }
}

bool imageHolds(Core &core, GuestImage image, std::uint32_t address) {
  return execution(core).activeKey(image, address).has_value();
}

void callOriginal(Core &core, GuestImage image, std::uint32_t address) {
  GuestExecution &context = execution(core);
  const std::uint32_t returnPc = core.r[31];
  const auto key = context.activeKey(image, address);
  if (!key || !core.nativeDispatcher().isInstalled(*key)) {
    lucent::error("crashbash-guest",
                  "original call at 0x{:08X} on image {} has no native owner in the current authenticated "
                  "image generation",
                  address,
                  imageName(image));
    std::abort();
  }
  const std::uint32_t value = runGuestCallToReturn(core, address, returnPc, "Crash Bash original call", key);
  lucent::debug("crashbash-original",
                "target=0x{:08X} r2=0x{:08X} r4=0x{:08X} r5=0x{:08X} r6=0x{:08X} "
                "r7=0x{:08X} r8=0x{:08X} r16=0x{:08X} r17=0x{:08X} r18=0x{:08X} ra=0x{:08X}",
                address,
                value,
                core.r[4],
                core.r[5],
                core.r[6],
                core.r[7],
                core.r[8],
                core.r[16],
                core.r[17],
                core.r[18],
                core.r[31]);
}

} // namespace crashbash::runtime
