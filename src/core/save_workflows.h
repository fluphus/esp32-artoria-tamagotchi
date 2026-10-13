#pragma once
#include "save_manager.h"
void realignOwnerTimestamps(PetState& pet,uint32_t duration);
void realignImportedTimestamps(PetState& pet,uint32_t saved,uint32_t now,uint8_t day);
namespace save_workflow {
enum class Startup {NewGame,Loaded,Recovery};
inline Startup loadStartup(SaveManager& manager,PetState& pet) {
    if(!manager.initialized())return Startup::Recovery;
    if(!manager.hasSave())return Startup::NewGame;
    return manager.load(pet)==SAVE_OK?Startup::Loaded:Startup::Recovery;
}
SaveResult persistDevice(SaveManager& manager,const DeviceState& device);
SaveResult restoreOwner(SaveManager& manager,PetState& pet,DeviceState& device,uint32_t now,uint8_t day);
SaveResult commitImport(SaveManager& manager,PetState& pet,DeviceState& device,GalleryData& gallery,
                        uint8_t slot,const uint8_t* bytes,size_t size,uint32_t now,uint8_t day);
}
