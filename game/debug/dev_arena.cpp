#include "dev_arena.h"

#include "core.h"
#include "crashbash_guest.h"
#include "measured_guest_call.h"
#include "pad_input.h"

#include <cstdio>
#include <cstring>
#include <lucent/log.h>

namespace crashbash::debug {
namespace {

// The guest level table rejects unknown arena ids.
constexpr std::uint32_t kLevelTableBound = 32u;

constexpr std::uint32_t kPageBound = 16u;

std::uint16_t levelType(Core &core, std::uint32_t id) {
  return core.mem_r16(guest::kArenaTable + id * guest::kArenaRecordBytes);
}

// One page of the guest arena table: `count` sixteen-byte entries at `base`.
struct Page {
  std::uint32_t base = 0;
  std::uint32_t count = 0;
  bool valid = false;
};

Page arenaPage(Core &core, std::uint32_t page) {
  Page result;
  result.base = core.mem_r32(guest::kArenaPageTable + page * guest::kArenaPageStride);
  result.count = core.mem_r32(guest::kArenaPageCounts + page * guest::kArenaPageStride);
  // A page is valid only when its table points into the resident MENU image and its count is small.
  result.valid = result.count != 0 && result.count <= 8u && result.base >= guest::kMenuImageBase &&
                 result.base < guest::kMenuImageBase + guest::kMenuImageBytes;
  return result;
}

// Address of the flow-table entry that requests `screen`; FUN_800b5360 takes an entry address and 0x800B5410
// walks the rest. Scoped to the MENU image so a stray equal word cannot match.
std::uint32_t findFlowEntry(Core &core, std::uint32_t screen) {
  for (std::uint32_t at = guest::kMenuImageBase; at + 8u <= guest::kMenuImageBase + guest::kMenuImageBytes; at += 4u) {
    if (core.mem_r32(at) != screen) {
      continue;
    }
    const std::uint32_t record = core.mem_r32(at);
    if (record < guest::kMenuImageBase || record >= guest::kMenuImageBase + guest::kMenuImageBytes) {
      continue;
    }
    if (core.mem_r32(record + 4u) == guest::kSelectBattleTypeUpdate || record == guest::kTournamentMatchScreen) {
      return at;
    }
  }
  return 0;
}

struct Entry {
  bool found = false;
  std::uint32_t page = 0;
  std::uint32_t index = 0;
  std::uint32_t mode = 0;
};

Entry findEntry(Core &core, std::uint32_t arena, std::uint32_t wantedMode) {
  Entry found;
  for (std::uint32_t page = 0; page < kPageBound; ++page) {
    const Page descriptor = arenaPage(core, page);
    if (!descriptor.valid) {
      break;
    }
    for (std::uint32_t index = 0; index < descriptor.count; ++index) {
      const std::uint32_t entry = descriptor.base + index * guest::kArenaEntryBytes;
      const std::uint32_t mode = core.mem_r32(entry);
      if (core.mem_r32(entry + 4u) == arena && (wantedMode == 0u || mode == wantedMode)) {
        found = Entry{true, page, index, mode};
        return found;
      }
    }
  }
  return found;
}

} // namespace

bool DevArena::handle(Core &core, const char *cmd, const char *line, std::FILE *out) {
  if (std::strcmp(cmd, "arena") != 0) {
    return false;
  }

  char argument[32] = {0};
  char mode[32] = {0};
  const int fields = std::sscanf(line, "%*31s %31s %31s", argument, mode);

  if (fields < 1 || std::strcmp(argument, "list") == 0) {
    const Page first = arenaPage(core, 0);
    if (!first.valid) {
      std::fprintf(out,
                   "the menu overlay is not resident (0x%08X holds 0x%08X): its arena table at 0x%08X is only readable "
                   "while the menu owns the slot\n",
                   guest::kMenuImageBase,
                   core.mem_r32(guest::kMenuImageBase),
                   guest::kArenaPageTable);
      return true;
    }
    std::fprintf(out, "menu arena table at 0x%08X / counts 0x%08X\n", guest::kArenaPageTable, guest::kArenaPageCounts);
    std::uint32_t listed = 0;
    for (std::uint32_t page = 0; page < kPageBound; ++page) {
      const Page descriptor = arenaPage(core, page);
      if (!descriptor.valid) {
        break;
      }
      for (std::uint32_t index = 0; index < descriptor.count; ++index) {
        const std::uint32_t entry = descriptor.base + index * guest::kArenaEntryBytes;
        std::fprintf(out,
                     "  arena %2u  page %u index %u  mode %u  name 0x%08X  str 0x%08X  type 0x%04X\n",
                     core.mem_r32(entry + 4u),
                     page,
                     index,
                     core.mem_r32(entry),
                     core.mem_r32(entry + 8u),
                     core.mem_r32(entry + 12u),
                     levelType(core, core.mem_r32(entry + 4u)));
        ++listed;
      }
    }
    std::fprintf(out, "%u arena entry(s)\n", listed);
    return true;
  }

  unsigned requested = 0;
  if (std::sscanf(argument, "%u", &requested) != 1) {
    std::fprintf(out, "usage: arena list | arena <id> [battle|tournament]\n");
    return true;
  }
  if (requested >= kLevelTableBound || levelType(core, requested) == 0) {
    std::fprintf(out, "refused: arena %u is not in the guest's level table at 0x%08X\n", requested, guest::kArenaTable);
    return true;
  }

  ArenaMode selected = ArenaMode::Battle;
  std::uint32_t wantedMode = 0;
  if (fields >= 2 && mode[0] != 0) {
    if (std::strcmp(mode, "battle") == 0) {
      selected = ArenaMode::Battle;
      wantedMode = 1;
    } else if (std::strcmp(mode, "tournament") == 0) {
      selected = ArenaMode::Tournament;
      wantedMode = 2;
    } else {
      std::fprintf(out, "refused: unknown mode '%s' (battle or tournament)\n", mode);
      return true;
    }
  }

  const Entry found = findEntry(core, requested, wantedMode);
  if (found.found) {
    request_ = Request{true, requested, selected, found.page, found.index, Stage::EnterMenu, 0, false, 0};
    std::fprintf(out,
                 "armed: arena %u (%s, table page %u index %u) — the frame driver will run the game's own menu flow "
                 "and select it\n",
                 requested,
                 selected == ArenaMode::Battle ? "battle" : "tournament",
                 found.page,
                 found.index);
    return true;
  }

  // A nested gameplay module shares the MENU load address, so mid-match the table is absent and the request
  // stays armed until the menu owns the slot again.
  request_ = Request{true, requested, selected, 0, 0, Stage::EnterMenu, 0, false, 0};
  std::fprintf(out,
               "armed: arena %u (%s) — the menu overlay is not resident right now, so the frame driver will select it "
               "when the menu returns\n",
               requested,
               selected == ArenaMode::Battle ? "battle" : "tournament");
  return true;
}

void DevArena::applyArmed(Core &core, Pad &pad, std::uint32_t frame) {
  if (!request_.armed) {
    return;
  }
  if (request_.firstFrame == 0) {
    request_.firstFrame = frame;
  }
  if (frame - request_.firstFrame > kFrameBudget) {
    lucent::info(
        "crashbash-arena",
        "arena {} abandoned at f{} after {} frame(s): current menu screen 0x{:08X}, guest arena selection 0x{:08X}",
        request_.arena,
        frame,
        kFrameBudget,
        core.mem_r32(guest::kMenuSceneTransition),
        core.mem_r32(guest::kArenaSelection));
    request_ = Request{};
    return;
  }

  const std::uint32_t tournament = request_.mode == ArenaMode::Tournament;
  // The flow is the run from SELECT GAME TYPE to the match, via SELECT BATTLE TYPE or the tournament screen.
  const std::uint32_t flow =
      findFlowEntry(core, tournament ? guest::kTournamentMatchScreen : guest::kSelectBattleTypeBattleScreen);
  const std::uint32_t screen = core.mem_r32(guest::kMenuSceneTransition);
  // SELECT BATTLE TYPE is matched by update pointer; two screen records share it.
  const bool atArenaScreen = screen != 0 && core.mem_r32(screen + 4u) == guest::kSelectBattleTypeUpdate &&
                             core.mem_r32(screen) == guest::kSelectBattleTypeEnter;

  if (screen != request_.lastScreen) {
    request_.lastScreen = screen;
    lucent::info("crashbash-arena",
                 "arena {} at f{}: menu screen 0x{:08X}{}",
                 request_.arena,
                 frame,
                 screen,
                 atArenaScreen ? " (SELECT BATTLE TYPE)" : "");
  }

  switch (request_.stage) {
  case Stage::EnterMenu: {
    const Page resident = arenaPage(core, 0);
    if (resident.valid) {
      request_.stage = Stage::SelectMode;
      return;
    }
    if (!request_.tapped && pad.repl_on != 0) {
      lucent::info("crashbash-arena",
                   "arena {} not applied at f{}: a pad replay owns input, so Start cannot leave the attract demo",
                   request_.arena,
                   frame);
      request_ = Request{};
      return;
    }
    const std::uint32_t waited = frame - request_.firstFrame;
    if (waited % kStartRetryStride == 0) {
      if (request_.tapped) {
        // Release the pad so host input works again.
        pad.driveRelease();
        request_.tapped = false;
      }
      pad.driveTap(kStartPressed, kPressFrames);
      request_.tapped = true;
      lucent::info(
          "crashbash-arena",
          "arena {} at f{}: the menu overlay is not resident, so Start was pressed through the pad to leave the "
          "attract demo (arena page 0 is 0x{:08X} with {} entry/entries)",
          request_.arena,
          frame,
          resident.base,
          resident.count);
    }
    return;
  }

  case Stage::SelectMode: {
    // Residency means the table is readable: the live screen record keeps its last MENU pointer after the
    // module is replaced, so it cannot test residency.
    const Page first = arenaPage(core, 0);
    if (!first.valid) {
      if (frame - request_.firstFrame >= kWaitReportStride) {
        lucent::info("crashbash-arena",
                     "arena {} waiting at f{} for the MENU overlay: arena page 0 is 0x{:08X} with {} entry/entries",
                     request_.arena,
                     frame,
                     first.base,
                     first.count);
      }
      return;
    }
    if (flow == 0) {
      lucent::info("crashbash-arena",
                   "arena {} ({}) abandoned at f{}: the menu overlay has no flow table reaching 0x{:08X}",
                   request_.arena,
                   tournament ? "tournament" : "battle",
                   frame,
                   tournament ? guest::kTournamentMatchScreen : guest::kSelectBattleTypeBattleScreen);
      request_ = Request{};
      return;
    }
    const Entry found = findEntry(core, request_.arena, tournament ? 2u : 1u);
    if (!found.found) {
      lucent::info("crashbash-arena",
                   "arena {} ({}) abandoned at f{}: the menu's own table offers no such entry (page 0 is 0x{:08X} with "
                   "{} entry/entries)",
                   request_.arena,
                   tournament ? "tournament" : "battle",
                   frame,
                   first.base,
                   first.count);
      request_ = Request{};
      return;
    }
    request_.page = found.page;
    request_.index = found.index;
    // Cursor is SELECT GAME TYPE's selection; 0x800B5360 stores the flow table in 0x800B9620 and requests the
    // transition.
    core.mem_w32(guest::kModeCursor, tournament ? guest::kModeCursorTournament : guest::kModeCursorBattle);
    measuredGuestCall(core, guest::kModeAccept, guest::kModeAcceptReturnPc, 4u, flow);
    request_.stage = Stage::AwaitArenaScreen;
    lucent::info("crashbash-arena",
                 "arena {} at f{}: mode cursor {} and the game's own flow step 0x{:08X} ran with flow 0x{:08X}, from "
                 "menu screen 0x{:08X} (table page {} index {})",
                 request_.arena,
                 frame,
                 tournament ? guest::kModeCursorTournament : guest::kModeCursorBattle,
                 guest::kModeAccept,
                 flow,
                 screen,
                 request_.page,
                 request_.index);
    return;
  }

  case Stage::AwaitArenaScreen: {
    // DAT_8005a64a/b are the arena cursor SELECT BATTLE TYPE indexes PTR_DAT_800ba324 with; held for the whole walk.
    core.mem_w8(guest::kArenaCursorPage, static_cast<std::uint8_t>(request_.page));
    core.mem_w8(guest::kArenaCursorIndex, static_cast<std::uint8_t>(request_.index));
    // Screens advance on the game's own accept, a Cross press.
    if ((frame - request_.firstFrame) % kFlowPressStride == 0) {
      pad.driveTap(kConfirmPressed, kPressFrames);
    }
    if (core.mem_r32(guest::kArenaSelection) == request_.arena && request_.stage == Stage::AwaitArenaScreen) {
      request_.stage = Stage::AwaitCommit;
      lucent::info("crashbash-arena",
                   "arena {} ({}) committed at f{} by the game's own menu flow: guest arena selection 0x{:08X}, "
                   "arena cursor {:02X}/{:02X}, on menu screen 0x{:08X}",
                   request_.arena,
                   tournament ? "tournament" : "battle",
                   frame,
                   core.mem_r32(guest::kArenaSelection),
                   core.mem_r8(guest::kArenaCursorPage),
                   core.mem_r8(guest::kArenaCursorIndex),
                   screen);
    }
    return;
  }

  case Stage::AwaitCommit: {
    // Arena set; the remaining screens advance by the same presses.
    if ((frame - request_.firstFrame) % kFlowPressStride == 0) {
      pad.driveTap(kConfirmPressed, kPressFrames);
    }
    const bool leftMenu = screen < guest::kMenuImageBase || screen >= guest::kMenuImageBase + guest::kMenuImageBytes;
    if (leftMenu) {
      const Request served = request_;
      request_ = Request{};
      lucent::info("crashbash-arena",
                   "arena {} ({}) entered at f{}: the menu flow left the overlay at menu screen 0x{:08X} with guest "
                   "arena selection 0x{:08X}",
                   served.arena,
                   tournament ? "tournament" : "battle",
                   frame,
                   screen,
                   core.mem_r32(guest::kArenaSelection));
    }
    return;
  }
  }
}

} // namespace crashbash::debug