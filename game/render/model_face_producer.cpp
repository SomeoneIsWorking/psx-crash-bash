#include "model_face_producer.h"

#include "component_incarnation.h"
#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "guest_execution.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crashbash::render {
namespace {

void modelDraw(Core *core) {
  const auto object = core->emission.instance(frameDriver(*core).componentIncarnations().object(core->r[7]));
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kModelDraw);
}

void meshFaceEmit(Core *core) {
  const std::uint32_t packets = core->r[4];
  const std::uint32_t faceList = core->r[6];
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kMeshFaceEmit);
  // Only FUN_80019A60 calls this; without its scope there is no object to name faces of.
  if (!core->emission.isOpen()) {
    return;
  }
  const std::uint32_t faces = meshFaceCount(*core, faceList);
  if (faces > kFaceIndexLimit) {
    lucent::error("crashbash-render",
                  "face list 0x{:08X} has {} faces; elements name at most {}",
                  faceList,
                  faces,
                  kFaceIndexLimit);
    std::abort();
  }
  for (std::uint32_t face = 0; face < faces; ++face) {
    const auto element = core->emission.element(meshFaceElement(faceList, face));
    core->emission.bindPacket(packets + face * kFacePacketBytes);
  }
}

} // namespace

std::uint32_t meshFaceCount(Core &core, std::uint32_t faceList) {
  std::uint32_t faces = 0;
  for (std::uint32_t strip = faceList;; strip += 2u) {
    const std::uint32_t count = core.mem_r8(strip + 1u);
    if (count == 0xFFu) {
      return faces;
    }
    faces += count;
  }
}

void registerModelFaceProducer(Core &core) {
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Resident,
                                  guest::kModelDraw,
                                  "CrashBash::ModelDraw",
                                  modelDraw,
                                  psx::present::Producer{psx::present::Arg::A3});
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kMeshFaceEmit, "CrashBash::MeshFaceEmit", meshFaceEmit);
}

} // namespace crashbash::render
