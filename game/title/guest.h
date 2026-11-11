#pragma once

#include "guest_address_range.h"

#include <cstdint>

namespace crashbash::guest {

// SCUS_945.70 guest addresses; the executable identity is authenticated before they are used.
inline constexpr GuestAddressRange kVSync{0x800320ECu, 0x800320F0u};

inline constexpr std::uint32_t kGameMain = 0x8002718Cu;
inline constexpr std::uint32_t kApplicationMain = 0x80010158u;
inline constexpr std::uint32_t kCdFileRead = 0x80027790u;
inline constexpr std::uint32_t kCdLicenseStartup = 0x8002D4F4u;
inline constexpr std::uint32_t kDisplayFrame = 0x800272ACu;
inline constexpr std::uint32_t kGpuTimeoutArm = 0x8003126Cu;
inline constexpr std::uint32_t kGpuTimeoutCheck = 0x800312A0u;
inline constexpr std::uint32_t kGpuTransfer = 0x8003165Cu;
// The GPU driver's ClearOTagR body (reached through ClearOTagR 0x8002F254's driver table): the OTC DMA.
inline constexpr std::uint32_t kGpuClearTable = 0x80030220u;
inline constexpr std::uint32_t kGteInitialization = 0x80033494u;
inline constexpr std::uint32_t kCdDriveReady = 0x800349ACu;
inline constexpr std::uint32_t kCdInitHandshake = 0x80034B8Cu;
inline constexpr std::uint32_t kCdSearchFile = 0x80034C6Cu;
inline constexpr std::uint32_t kCdSync = 0x8003E6B0u;
inline constexpr std::uint32_t kCdCommand = 0x8003EBF8u;
inline constexpr std::uint32_t kMemoryCardStartup = 0x800486DCu;

// The two ordering tables DisplayFrame alternates between (pointer at 0x8005B68C): 0x1000 buckets of one
// word, cleared by ClearOTagR so each bucket links to the one below it, and walked from the last bucket
// down. The packet pool cursor of the table in use is at +0x4008.
inline constexpr std::uint32_t kOrderingTablePointer = 0x8005B68Cu;
inline constexpr std::uint32_t kOrderingTableA = 0x8005B790u;
inline constexpr std::uint32_t kOrderingTableB = 0x8005F79Cu;
inline constexpr std::uint32_t kOrderingTableBuckets = 0x1000u;
inline constexpr std::uint32_t kOrderingTablePoolCursor = 0x4008u;

// FUN_80028F28(scroller, owner): scrolls a texture strip with two DR_MOVEs linked at the last bucket's head, in the
// update (the Polar Push aurora).
inline constexpr std::uint32_t kTextureScroll = 0x80028F28u;

// The ordering-table bucket the current draw slice links from.
inline constexpr std::uint32_t kDrawOtBase = 0x800569D8u;

// Model draw: FUN_80019A60(frame code, model data, flags, component), reached from the standard
// (0x80019F1C) and alternate (0x8001DD50) object draw callbacks.
inline constexpr std::uint32_t kModelDraw = 0x80019A60u;

// A render component (0x80019A60's a3) is 0xA8 bytes, chained through +0x5C. It begins a new life
// when one of these copies the template 0x8005A830 into it; they are the template's only copiers.
inline constexpr std::uint32_t kRenderComponentBytes = 0xA8u;
inline constexpr std::uint32_t kRenderComponentNext = 0x5Cu;
// Pool allocators (entity, count) / (head, tail, count): take the first v0 components of this pool.
inline constexpr std::uint32_t kRenderComponentPool = 0x80058ACCu;
inline constexpr std::uint32_t kEntityComponentAlloc = 0x80018F3Cu;
inline constexpr std::uint32_t kListComponentAlloc = 0x8001D48Cu;
// (entity, count, buffer): fills `count` consecutive components of the caller's buffer.
inline constexpr std::uint32_t kBufferComponentInit = 0x8001D3B0u;
// (descriptor): fills descriptor+0x18 consecutive components at descriptor+0x1C; the first
// allocates that array, the second reuses it.
inline constexpr std::uint32_t kPlacedComponentAlloc = 0x8001D6B4u;
inline constexpr std::uint32_t kPlacedComponentInit = 0x8001E0A8u;
inline constexpr std::uint32_t kPlacedComponentCount = 0x18u;
inline constexpr std::uint32_t kPlacedComponentArray = 0x1Cu;
// Animation node creators for node types 0, 3 and 5: pop a node off this list; its component is at +8.
inline constexpr std::uint32_t kAnimationNodeFreeList = 0x80058B70u;
inline constexpr std::uint32_t kAnimationNodeComponent = 8u;
inline constexpr std::uint32_t kAnimationNodeCreateType0 = 0x80021A1Cu;
inline constexpr std::uint32_t kAnimationNodeCreateType3 = 0x80021798u;
inline constexpr std::uint32_t kAnimationNodeCreateType5 = 0x8002128Cu;
// BOOT: 0x8007EB68 initialises the static component 0x8009AD94; 0x8009440C re-initialises its scratch
// component 0x800A0C74 for each entry it draws.
inline constexpr std::uint32_t kBootStaticComponentInit = 0x8007EB68u;
inline constexpr std::uint32_t kBootStaticComponent = 0x8009AD94u;
inline constexpr std::uint32_t kBootScratchComponent = 0x800A0C74u;
// Render component draw callbacks (+0x54), the component in a0: text (FUN_8001A82C, the string at +0x6C placed by the
// position), panel (FUN_8001A6D4 over its +0x6C record) and quad (the shaded quad 0x8001A0D8 over it), each under the
// component's transform unless flag 0x10000000 is set.
inline constexpr std::uint32_t kTextComponentDraw = 0x8001C448u;
inline constexpr std::uint32_t kPanelComponentDraw = 0x8001C690u;
inline constexpr std::uint32_t kQuadComponentDraw = 0x8001C7FCu;

// FUN_8001ACC0(component): the matrix 0x80056A44 a root component's children draw by (entity draws, animation trees).
inline constexpr std::uint32_t kRootTransform = 0x8001ACC0u;

// Load queue pacing pump: one queue step per call. Retail pending counter 0x80012FFC is zero exactly
// when the three words below are zero; the swap path 0x8001E610 loops the pump while it is nonzero.
inline constexpr std::uint32_t kLoadPump = 0x8001231Cu;
inline constexpr std::uint32_t kLoadCooldownWord = 0x80050628u;
inline constexpr std::uint32_t kLoadReadActiveWord = 0x8005062Cu;
inline constexpr std::uint32_t kLoadQueueHeadWord = 0x80050634u;

// BOOT logo controller; on completion it requests kBootLogoHandoffState with kBootLogoHandoffFlags.
inline constexpr std::uint32_t kBootLogoUpdate = 0x8008E5BCu;
inline constexpr std::uint32_t kSceneTransitionRequest = 0x8001E588u;
inline constexpr std::uint32_t kBootLogoHandoffState = 0x800A00DCu;
inline constexpr std::uint32_t kBootLogoHandoffFlags = 0x00000012u;

inline constexpr std::uint32_t kInitialProcessState = 0x8004E0B8u;
inline constexpr std::uint32_t kCurrentProcessState = 0x8005B648u;
inline constexpr std::uint32_t kInitialStateEnter = 0x80010410u;
inline constexpr std::uint32_t kInitialStateUpdate = 0x80010394u;
inline constexpr std::uint32_t kInitialStatePresent = 0x80010278u;

// Pointer to the shell's root scene {enter, update, present} (+0/+4/+8), arena at +0x10. Written once
// at 0x800101CC with BOOT's load address; it is not a mode selector (see kSceneTransition).
inline constexpr std::uint32_t kAppModeVtable = 0x8004E0DCu;

// The mode-selecting scene record {current, target, previous, flags}: 0x8001E610 reads it,
// 0x8001E588 writes target (+4) and flags (+0xC). The clock is {value, step, age, flags}.
inline constexpr std::uint32_t kSceneTransition = 0x8009F658u;
inline constexpr std::uint32_t kSceneTransitionClock = 0x8009F644u;
// A scene is {enter, update, present, arena}; a scene record is {current, target, previous, flags}.
inline constexpr std::uint32_t kSceneTransitionEnterSlot = 0u;
inline constexpr std::uint32_t kSceneTransitionUpdateSlot = 4u;
inline constexpr std::uint32_t kSceneTransitionPresentSlot = 8u;
inline constexpr std::uint32_t kSceneCurrentSlot = 0u;
inline constexpr std::uint32_t kSceneTargetSlot = 4u;
inline constexpr std::uint32_t kScenePreviousSlot = 8u;
inline constexpr std::uint32_t kSceneFlagsSlot = 12u;
inline constexpr std::uint32_t kSceneClockAgeSlot = 8u;

// BOOT's mode scene {enter 0x80092CAC, update 0x80092EDC, present 0x80092E94}: its update runs the pause
// dispatcher FUN_8007F314 first. The dispatcher's state word is 0x8005A624; bit 0 is "pause page open".
inline constexpr std::uint32_t kModeScene = 0x8009F720u;
inline constexpr std::uint32_t kPauseFlags = 0x8005A624u;
inline constexpr std::uint32_t kPauseOpenBit = 0x1u;

// Second scene record for the menu world, run by 0x8001E610 from FUN_80092EDC. Current is the active
// menu screen struct (SELECT GAME TYPE is 0x800B8E28).
inline constexpr std::uint32_t kMenuSceneTransition = 0x8009F8A4u;

// FUN_80018B08(viewport, context): makes `context` the current draw context (0x800569E0), derives its view
// (FUN_80014540) and links the viewport's draw environment. BOOT's handoff presents FUN_8009421C (record 0x8009F998)
// and FUN_8009414C (record 0x8009FA00) draw the LOADING card through it.
inline constexpr std::uint32_t kSelectDrawContext = 0x80018B08u;
// FUN_80014540(context): the view of `context` from its translation, eye, H and angles; called from 0x80018B08 and
// FUN_8002AEE0.
inline constexpr std::uint32_t kDeriveView = 0x80014540u;

// Arena read by FUN_800793E8; MENU fills it from the SELECT BATTLE TYPE cursor (FUN_800b7ef4). The
// dispatcher FUN_8007F314 needs the scene machine's prepared frame, so $a1 is not the arena index;
// the arena goes in through the accept's flow-table entry (dev_arena.cpp).
inline constexpr std::uint32_t kArenaSelection = 0x8009E5DCu;
// Six-byte arena records: level type FUN_800793e8 switches on (0x104 Crashball, 0x106..0x10E arena
// families), per-family slot, the game's arena id.
inline constexpr std::uint32_t kArenaTable = 0x8004DDD0u;
inline constexpr std::uint32_t kArenaRecordBytes = 6u;

// Menu mode flow: 0x800B3CA8 (SELECT GAME TYPE update) calls 0x800B5360, which stores the flow table
// in 0x800B9620 and requests the transition from record 0x8009F8A4; 0x800B5410 advances it one
// entry per screen accept. 0x800B5360 makes no guest call, so it never returns to its boundary.
inline constexpr std::uint32_t kModeAccept = 0x800B5360u;
inline constexpr std::uint32_t kModeAcceptReturnPc = 0x800B53C8u;
// A mode's flow table is a run of screen records starting at SELECT GAME TYPE: Battle at 0x800B8EA0,
// Tournament at 0x800B8ED0. The attract demo's short flow at 0x800B8EC0 skips the arena screen, so
// the table is located by the screen its fourth entry reaches.
inline constexpr std::uint32_t kSelectBattleTypeBattleScreen = 0x800BA72Cu;
inline constexpr std::uint32_t kTournamentMatchScreen = 0x8009EB84u;
// SELECT BATTLE TYPE has two identical screen records (0x800BA72C, 0x800BAF2C); match on the update pointer.
inline constexpr std::uint32_t kSelectBattleTypeUpdate = 0x800B7458u;
inline constexpr std::uint32_t kSelectBattleTypeEnter = 0x800B6CF4u;
// MENU overlay range (titles/crashbash/menu_module.json).
inline constexpr std::uint32_t kMenuImageBase = 0x800B32B4u;
inline constexpr std::uint32_t kMenuImageBytes = 0x00008000u;
inline constexpr std::uint32_t kModeCursor = 0x800B95F0u;
inline constexpr std::uint32_t kModeCursorBattle = 0u;
inline constexpr std::uint32_t kModeCursorTournament = 2u;
inline constexpr std::uint32_t kArenaCursorPage = 0x8005A64Au;
inline constexpr std::uint32_t kArenaCursorIndex = 0x8005A64Bu;
inline constexpr std::uint32_t kArenaPageTable = 0x800BA324u;
inline constexpr std::uint32_t kArenaPageCounts = 0x800BA328u;
inline constexpr std::uint32_t kArenaEntryBytes = 0x10u;
inline constexpr std::uint32_t kArenaPageStride = 0x8u;

inline constexpr std::uint32_t kDisplayFieldsPerFrame = 0x8004E0E0u;
inline constexpr std::uint32_t kVblankCounter = 0x8006D8DCu;
inline constexpr std::uint32_t kCdReadActive = 0x800637B4u;
inline constexpr std::uint32_t kCdBaseLba = 0x800637B8u;
inline constexpr std::uint32_t kCdLicenseState = 0x80067894u;
inline constexpr std::uint32_t kDiscExecutableLba = 0x00000017u;
inline constexpr std::uint32_t kDiscExecutableSize = 0x00069800u;
inline constexpr std::uint32_t kDiscSystemCnfLba = 0x000000EAu;
inline constexpr std::uint32_t kDiscSystemCnfSize = 0x00000045u;
inline constexpr std::uint32_t kDiscDataLba = 0x000000ECu;
inline constexpr std::uint32_t kDiscDataSize = 0x045D4000u;
inline constexpr std::uint32_t kDiscTrackSectors = 0x000127FEu;

} // namespace crashbash::guest
