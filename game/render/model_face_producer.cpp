#include "render/model_face_producer.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "render/component_incarnation.h"
#include "render/face_projection.h"
#include "render/model_state.h"
#include "render/packet_collector.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crashbash::render {
namespace {

void modelDraw(Core *core) {
  const auto object = core->emission.instance(session(*core).componentIncarnations.object(core->r[7]));
  PacketCollector::Scope packets(session(*core).packetCollector);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kModelDraw);
  packets.save(*core);
}

void meshFaceEmit(Core *core) {
  // Only FUN_80019A60 calls this; without its scope there is no object to name faces of.
  const bool named = core->emission.isOpen();
  FaceCall &call = session(*core).packetCollector.faceCall(named);
  readFaceCall(*core, call);
  GuestFaceSink sink(*core, FaceTarget{call.packets, call.globals.otBase}, call.linked);
  projectFaces(*core, FaceInputs{call.list, call.vertices, call.globals.zBias, call.globals.zLimit}, sink);
  // The body ends on the count that terminated the list, in both result registers.
  core->r[2] = kFaceListEnd;
  core->r[3] = kFaceListEnd;
  if (!named) {
    return;
  }
  const std::uint32_t faceList = core->r[6];
  const std::uint32_t faces = faceCount(call.list);
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
    core->emission.bindPacket(call.packets + face * kFacePacketBytes);
  }
}

} // namespace

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
