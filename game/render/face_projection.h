// game/render/face_projection.h — FUN_800193A8: a mesh's faces projected through the GTE and linked into the
// ordering table.
//
// The guest function takes a packet buffer (a0, faces 40 bytes apart), vertices (a1, two words each: x/y
// halves, then z with face flags above it) and a face list (a2, {flags, count} strips ended by a count of
// 0xFF). Each strip projects its first two vertices, then one more per face, sums the last three depths into
// the bucket and culls by NCLIP and the vertex flags; a visible face takes the projected points as its
// vertices and links into its bucket. `projectFaces` is that body over the GTE and the inputs alone: the
// guest override gives it the live registers and writes each face into guest memory, a render gives it saved
// registers and reads each face from the sink.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace crashbash::render {

// Face flags 0 and 1 pick the packet shape: a textured triangle whose vertices sit at words 2, 5, 8 and whose
// length the body sets, or a Gouraud triangle behind a draw mode word, vertices at words 4, 6, 8.
enum class FaceLayout : std::uint8_t { Textured, Shaded };

inline constexpr std::uint32_t kFacePacketWords = 10u;
// The count that ends a face list.
inline constexpr std::uint32_t kFaceListEnd = 0xFFu;

// Word offsets in a packet of its three projected points.
std::array<std::uint32_t, 3> facePointWords(FaceLayout layout);

struct FaceEmit {
  std::uint32_t face = 0;
  FaceLayout layout = FaceLayout::Textured;
  std::uint32_t bucket = 0; // relative to the table slice base
};

// Takes each visible face as the body reaches it, while the GTE still holds its projected points.
class FaceSink {
public:
  virtual ~FaceSink() = default;
  virtual void emit(const FaceEmit &face) = 0;
};

struct FaceInputs {
  std::span<const std::uint8_t> list;
  std::span<const std::uint32_t> vertices;
  std::int32_t zBias = 0;
  std::int32_t zLimit = 0;
};

// The face list at `address`, ended by its 0xFF count, into `list`.
void readFaceList(Core &core, std::uint32_t address, std::vector<std::uint8_t> &list);
// Vertex words the body reads for `list`.
std::uint32_t faceVertexWords(std::span<const std::uint8_t> list);
// Faces `list` names.
std::uint32_t faceCount(std::span<const std::uint8_t> list);

void projectFaces(Core &core, const FaceInputs &inputs, FaceSink &sink);

// Where the guest function writes faces: its packet buffer and its ordering table slice.
struct FaceTarget {
  std::uint32_t packets = 0;
  std::uint32_t otBase = 0;
};

// Writes each visible face into the target's packet buffer and links it into its table slice as the guest
// function does, and appends each face it links to `linked`.
class GuestFaceSink final : public FaceSink {
public:
  GuestFaceSink(Core &core, FaceTarget target, std::vector<FaceEmit> &linked)
      : core_(core), target_(target), linked_(linked) {}

  void emit(const FaceEmit &face) override;

private:
  Core &core_;
  FaceTarget target_;
  std::vector<FaceEmit> &linked_;
};

} // namespace crashbash::render
