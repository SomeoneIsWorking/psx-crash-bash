#include "leaf_port.h"

#include "core.h"
#include "crashbash_guest.h"
#include "emit_memory.h"
#include "guest_call.h"

namespace crashbash::render {
namespace {

// The dead stack the callee's frame sits in: its outgoing arguments, then a shaded quad's vertices.
constexpr std::uint32_t kFrameBytes = 0x60u;
constexpr std::uint32_t kStackArguments = 16u;
constexpr std::uint32_t kVertexOffset = 0x20u;

// Calls made as the body: below its stack, returning to its own address.
class CallFrame {
public:
  CallFrame(Core &core, std::uint32_t owner) : core_(core), stack_(core.r[29]), link_(core.r[31]) {
    core_.r[29] = stack_ - kFrameBytes;
    core_.r[31] = owner;
  }
  ~CallFrame() {
    core_.r[29] = stack_;
    core_.r[31] = link_;
  }
  CallFrame(const CallFrame &) = delete;
  CallFrame &operator=(const CallFrame &) = delete;
  CallFrame(CallFrame &&) = delete;
  CallFrame &operator=(CallFrame &&) = delete;

  std::uint32_t argument(std::uint32_t index) const {
    return core_.r[29] + kStackArguments + index * 4u;
  }
  std::uint32_t vertices() const {
    return core_.r[29] + kVertexOffset;
  }

private:
  Core &core_;
  std::uint32_t stack_;
  std::uint32_t link_;
};

LeafExecution resultOf(const Core &core) {
  LeafExecution done;
  done.value = core.r[2];
  done.second = core.r[3];
  done.setsSecond = true;
  return done;
}

} // namespace

LeafExecution GuestLeafPort::draw(LeafCall call, std::uint32_t element) {
  const psx::present::ElementScope scope(psx::present::EmitMemory(core_), element);
  const CallFrame frame(core_, owner_);
  switch (call.kind) {
  case LeafKind::Quad:
    for (std::uint32_t colour = 1; colour < 4u; ++colour) {
      core_.mem_w32(frame.argument(colour - 1u), call.colours[colour]);
    }
    psx::cpu::callGuestNow(core_,
                           "Crash Bash 2D body",
                           guest::kImageQuad,
                           call.textureAddress,
                           call.position,
                           call.bucket,
                           call.colours[0]);
    break;
  case LeafKind::Sprite:
    psx::cpu::callGuestNow(core_,
                           "Crash Bash 2D body",
                           guest::kImageSprite,
                           call.textureAddress,
                           call.position,
                           call.bucket,
                           call.colours[0]);
    break;
  case LeafKind::Shaded:
    for (std::uint32_t byte = 0; byte < call.vertices.size(); ++byte) {
      core_.mem_w8(frame.vertices() + byte, call.vertices[byte]);
    }
    psx::cpu::callGuestNow(core_, "Crash Bash 2D body", guest::kShadedQuad, frame.vertices(), call.attributes);
    break;
  }
  return resultOf(core_);
}

BodyResult GuestLeafPort::border(const BorderArguments &arguments) {
  const CallFrame frame(core_, owner_);
  core_.mem_w32(frame.argument(0), static_cast<std::uint32_t>(arguments.edgeHeight));
  psx::cpu::callGuestNow(core_,
                         "Crash Bash 2D body",
                         guest::kBorderDraw,
                         arguments.edge,
                         arguments.colour,
                         arguments.attributes,
                         static_cast<std::uint32_t>(arguments.sideWidth));
  BodyResult result;
  result.take(resultOf(core_));
  return result;
}

} // namespace crashbash::render
