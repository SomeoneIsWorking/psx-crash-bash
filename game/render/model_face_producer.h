// game/render/model_face_producer.h — keys the guest's model packets by the object that draws them.
//
// FUN_80019A60 draws one object (a3) by its frame code (a0) from model data (a1): a flat sprite through
// FUN_80029D28 for 0x3000 codes, otherwise a mesh whose faces FUN_800193A8 writes into a packet buffer
// that FUN_80019094 pops per (model frame, display parity). Objects drawing one model frame take those
// buffers in call order, so the buffer is not identity: the object's incarnation is
// (component_incarnation.h), and each face is an element named by its mesh's face list and its index in
// it. Face i's packet is at buffer + i * 40 whether or not it was culled.
#pragma once

#include <cstdint>

class Core;

namespace crashbash::render {

inline constexpr std::uint32_t kFacePacketBytes = 40u;
inline constexpr std::uint32_t kFaceIndexBits = 11u;
inline constexpr std::uint32_t kFaceIndexLimit = 1u << kFaceIndexBits;

// The face list's main-RAM offset above the face index; main RAM is 2 MB, so both fit one word.
constexpr std::uint32_t meshFaceElement(std::uint32_t faceList, std::uint32_t face) {
  return ((faceList & 0x1FFFFFu) << kFaceIndexBits) | face;
}

// FUN_80019A60 as the producer (object: a3's incarnation) and FUN_800193A8 as the namer of its faces.
void registerModelFaceProducer(Core &core);

} // namespace crashbash::render
