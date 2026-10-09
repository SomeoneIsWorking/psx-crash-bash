#include "component_body.h"

#include <lucent/log.h>

#include <cstdlib>
#include <type_traits>

namespace crashbash::render {
namespace {

[[noreturn]] void unknownKind(std::uint32_t kind) {
  lucent::error("component-body", "a saved component body of kind {}", kind);
  std::abort();
}

} // namespace

BodyResult runBody(const ComponentBody &body, const DrawGlobals &globals, LeafPort &port) {
  return std::visit(
      [&](const auto &draw) -> BodyResult {
        using Draw = std::decay_t<decltype(draw)>;
        if constexpr (std::is_same_v<Draw, StringBody>) {
          return runString(draw, globals, port).result;
        } else if constexpr (std::is_same_v<Draw, NumberBody>) {
          return runNumber(draw, globals, port);
        } else if constexpr (std::is_same_v<Draw, PanelBody>) {
          return runPanel(draw, globals, port);
        } else {
          return runBorder(draw, globals, port);
        }
      },
      body);
}

bool sameKind(const ComponentBody &from, const ComponentBody &to) {
  return from.index() == to.index();
}

ComponentBody blendBody(const ComponentBody &from, const ComponentBody &to, float t) {
  switch (to.index()) {
  case 0:
    return blendString(std::get<0>(from), std::get<0>(to), t);
  case 1:
    return blendNumber(std::get<1>(from), std::get<1>(to), t);
  case 2:
    return blendPanel(std::get<2>(from), std::get<2>(to), t);
  default:
    return blendBorder(std::get<3>(from), std::get<3>(to), t);
  }
}

void writeBody(psx::present::StateWriter &writer, const ComponentBody &body) {
  writer.put(static_cast<std::uint32_t>(body.index()));
  switch (body.index()) {
  case 0:
    writeString(writer, std::get<0>(body));
    break;
  case 1:
    writeNumber(writer, std::get<1>(body));
    break;
  case 2:
    writePanel(writer, std::get<2>(body));
    break;
  default:
    writeBorder(writer, std::get<3>(body));
    break;
  }
}

ComponentBody readBody(psx::present::StateReader &reader) {
  const auto kind = reader.get<std::uint32_t>();
  switch (kind) {
  case 0:
    return readString(reader);
  case 1:
    return readNumber(reader);
  case 2:
    return readPanel(reader);
  case 3:
    return readBorder(reader);
  default:
    unknownKind(kind);
  }
}

} // namespace crashbash::render
