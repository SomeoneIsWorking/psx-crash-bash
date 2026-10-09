// Guest code for tests: MIPS encoders.
#pragma once

#include "core.h"

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace crashbash::test {

inline constexpr std::uint32_t kT0 = 8u;
inline constexpr std::uint32_t kT1 = 9u;
inline constexpr std::uint32_t kT3 = 11u;
inline constexpr std::uint32_t kT4 = 12u;
inline constexpr std::uint32_t kS2 = 18u;
inline constexpr std::uint32_t kS3 = 19u;
inline constexpr std::uint32_t kS4 = 20u;
inline constexpr std::uint32_t kS5 = 21u;
inline constexpr std::uint32_t kT9 = 25u;
inline constexpr std::uint32_t kRa = 31u;

constexpr std::uint32_t nop() {
  return 0u;
}
constexpr std::uint32_t j(std::uint32_t target) {
  return 0x08000000u | ((target >> 2) & 0x3FFFFFFu);
}
constexpr std::uint32_t jal(std::uint32_t target) {
  return 0x0C000000u | ((target >> 2) & 0x3FFFFFFu);
}
constexpr std::uint32_t jr(std::uint32_t rs) {
  return (rs << 21) | 8u;
}
constexpr std::uint32_t move(std::uint32_t rd, std::uint32_t rs) {
  return (rs << 21) | (rd << 11) | 0x21u;
}
constexpr std::uint32_t addiu(std::uint32_t rt, std::uint32_t rs, std::uint32_t imm) {
  return 0x24000000u | (rs << 21) | (rt << 16) | (imm & 0xFFFFu);
}
constexpr std::uint32_t lui(std::uint32_t rt, std::uint32_t imm) {
  return 0x3C000000u | (rt << 16) | (imm & 0xFFFFu);
}
constexpr std::uint32_t lw(std::uint32_t rt, std::uint32_t rs, std::uint32_t offset = 0u) {
  return 0x8C000000u | (rs << 21) | (rt << 16) | (offset & 0xFFFFu);
}
constexpr std::uint32_t lbu(std::uint32_t rt, std::uint32_t rs) {
  return 0x90000000u | (rs << 21) | (rt << 16);
}
constexpr std::uint32_t sw(std::uint32_t rt, std::uint32_t rs, std::uint32_t offset = 0u) {
  return 0xAC000000u | (rs << 21) | (rt << 16) | (offset & 0xFFFFu);
}
constexpr std::uint32_t beqz(std::uint32_t rs, std::uint32_t pc, std::uint32_t target) {
  return 0x10000000u | (rs << 21) | (((target - (pc + 4u)) >> 2) & 0xFFFFu);
}

inline void place(Core &core, std::uint32_t address, std::initializer_list<std::uint32_t> words) {
  for (const std::uint32_t word : words) {
    core.mem_w32(address, word);
    address += 4u;
  }
}

} // namespace crashbash::test
