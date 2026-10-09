#include "native_owner_set.h"

#include "boot_logo_skip.h"
#include "boot_object_callbacks.h"
#include "cd_file_read.h"
#include "cd_license_startup.h"
#include "cd_startup.h"
#include "component_incarnation.h"
#include "display_frame.h"
#include "gpu_timeout.h"
#include "loading_card_skip.h"
#include "memory_card_startup.h"
#include "model_face_producer.h"
#include "ordering_table_slots.h"
#include "polar_push_contact.h"
#include "state_renders.h"
#include "ui_producer.h"

namespace crashbash {

void registerNativeOwners(Core &core) {
  registerCdFileReadOverride(core);
  registerLoadPumpDrainOverride(core);
  registerCdLicenseStartupOverride(core);
  registerCdStartupOverride(core);
  registerMemoryCardStartupOverride(core);
  registerGpuTimeoutOverrides(core);
  registerDisplayFrameOverride(core);
  registerBootObjectCallbackOverrides(core);
  registerBootLogoSkipOverride(core);
  registerLoadingCardSkipOverride(core);
  polar::registerPolarPushContactOverride(core);
  render::registerModelFaceProducer(core);
  render::registerComponentIncarnationOwners(core);
  render::registerUiProducers(core);
  render::nameOrderingTables(core);
  render::registerStateRenders(core);
}

} // namespace crashbash
