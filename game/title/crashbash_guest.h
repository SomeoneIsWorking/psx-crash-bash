#pragma once

#include "guest_program_image.h"

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

// What the packet bodies (the mesh face emitter and the 2D leaves) read: the display environment pointer
// (its s16 at +4 is the width the 640-column layout scales to), the screen fade, the table slice base,
// the depth bias and limit, and the 2D origin.
inline constexpr std::uint32_t kDrawEnvironment = 0x8005B698u;
inline constexpr std::uint32_t kEnvironmentScaleOffset = 4u;
inline constexpr std::uint32_t kScreenFade = 0x800569ACu;
inline constexpr std::uint32_t kDrawOtBase = 0x800569D8u;
inline constexpr std::uint32_t kDrawZBias = 0x800569DCu;
inline constexpr std::uint32_t kDrawZLimit = 0x800569DEu;
inline constexpr std::uint32_t kDrawOriginX = 0x800569C0u;
inline constexpr std::uint32_t kDrawOriginY = 0x800569C4u;

// Model draw: FUN_80019A60(frame code, model data, flags, object), reached from the standard
// (0x80019F1C) and alternate (0x8001DD50) object draw callbacks; FUN_800193A8 emits a mesh's faces.
inline constexpr std::uint32_t kModelDraw = 0x80019A60u;
inline constexpr std::uint32_t kMeshFaceEmit = 0x800193A8u;

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
inline constexpr std::uint32_t kBootScratchComponentDraw = 0x8009440Cu;
inline constexpr std::uint32_t kBootScratchComponent = 0x800A0C74u;

// UI component draw callbacks (component+0x54), each called with the component in a0: text (string at
// +0x6C through 0x8001A82C), panel (body and border quads through 0x8001A6D4), and screen quad.
inline constexpr std::uint32_t kTextComponentDraw = 0x8001C448u;
inline constexpr std::uint32_t kPanelComponentDraw = 0x8001C690u;
inline constexpr std::uint32_t kQuadComponentDraw = 0x8001C7FCu;
// 2D leaves: textured quad (image, packed xy, OT index, 4 colours), sprite, and the shaded quad.
inline constexpr std::uint32_t kImageQuad = 0x8002992Cu;
inline constexpr std::uint32_t kImageSprite = 0x80029D28u;
inline constexpr std::uint32_t kShadedQuad = 0x8001A0D8u;
// FUN_800243A0(x, y, string, ...) walks the string in s2; each glyph's leaf call returns to one of these
// with s2 one past the glyph's byte.
inline constexpr std::uint32_t kStringDraw = 0x800243A0u;
inline constexpr std::uint32_t kStringCursorRegister = 18u;
inline constexpr std::uint32_t kStringGlyphQuadReturn = 0x80024688u;
inline constexpr std::uint32_t kStringGlyphRaisedReturn = 0x800245F4u;
inline constexpr std::uint32_t kStringGlyphSpriteReturn = 0x80024640u;
// FUN_800248A0 draws a three-digit number, hundreds first.
inline constexpr std::uint32_t kNumberDraw = 0x800248A0u;
inline constexpr std::uint32_t kNumberHundredsReturn = 0x800249A0u;
inline constexpr std::uint32_t kNumberTensReturn = 0x80024A2Cu;
inline constexpr std::uint32_t kNumberUnitsReturn = 0x80024AACu;
// 0x8001A6D4 draws a panel's body; 0x8001A43C its left, right, top and bottom borders.
inline constexpr std::uint32_t kPanelBodyReturn = 0x8001A7E8u;
inline constexpr std::uint32_t kPanelLeftReturn = 0x8001A530u;
inline constexpr std::uint32_t kPanelRightReturn = 0x8001A5ACu;
inline constexpr std::uint32_t kPanelTopReturn = 0x8001A62Cu;
inline constexpr std::uint32_t kPanelBottomReturn = 0x8001A6ACu;
inline constexpr std::uint32_t kQuadComponentReturn = 0x8001C960u;
// BOOT HUD 0x800798A4: icon loops over 8-byte records in s0 (0x800996E0 and 0x8009AD54, four each), and
// per-player digits from the player's record in s4 (0x8009AD74 + 8 * player), units drawn first.
inline constexpr std::uint32_t kBootHudDraw = 0x800798A4u;
inline constexpr std::uint32_t kBootHudIconRegister = 16u;
inline constexpr std::uint32_t kBootHudDigitRegister = 20u;
inline constexpr std::uint32_t kBootHudIconReturns[] = {0x800799FCu, 0x80079B58u, 0x80079C5Cu};
inline constexpr std::uint32_t kBootHudThreeDigitReturns[] = {0x80079DF8u, 0x80079E6Cu, 0x80079EE0u};
inline constexpr std::uint32_t kBootHudTwoDigitReturns[] = {0x80079F9Cu, 0x8007A010u};
inline constexpr std::uint32_t kBootHudOneDigitReturn = 0x8007A0C8u;
// BOOT menu page 0x800809A0(items, y): walks 24-byte item records in s4 and draws each item's string or
// number from it.
inline constexpr std::uint32_t kBootMenuPageDraw = 0x800809A0u;
inline constexpr std::uint32_t kBootMenuItemRegister = 20u;
inline constexpr std::uint32_t kBootMenuStringReturns[] = {0x80080AC0u, 0x80080F30u};
inline constexpr std::uint32_t kBootMenuNumberReturn = 0x80080BC0u;

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

// Second scene record for the menu world, run by 0x8001E610 from FUN_80092EDC. Current is the active
// menu screen struct (SELECT GAME TYPE is 0x800B8E28).
inline constexpr std::uint32_t kMenuSceneTransition = 0x8009F8A4u;

// LOADING card draw; BOOT's handoff presents FUN_8009421C (record 0x8009F998) and FUN_8009414C
// (record 0x8009FA00) are its only callers while the handoff scene 0x800A00DC is current.
inline constexpr std::uint32_t kLoadingCardDraw = 0x80018B08u;

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
inline constexpr std::uint32_t kVblankRoot = 0x8003ADD4u;
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
