#include "save_workflows.h"
#include "save_io.h"
#include <string.h>
#include <initializer_list>
// 串门结束时: 对齐主人宠物的时间戳到当前设备时间
void realignOwnerTimestamps(PetState& ownerPet, uint32_t frozenDuration) {
    // 所有绝对时间戳 += frozenDuration, 使主人宠物的时间锚点对齐到当前设备时间
    if (ownerPet.idle_paused_until > 0)
        ownerPet.idle_paused_until += frozenDuration;
    if (ownerPet.last_poke_effect_time > 0)
        ownerPet.last_poke_effect_time += frozenDuration;
    if (ownerPet.rhongo_timer_start > 0)
        ownerPet.rhongo_timer_start += frozenDuration;
    if (ownerPet.birth_timestamp > 0)
        ownerPet.birth_timestamp += frozenDuration;
    if (ownerPet.last_interact_time > 0)
        ownerPet.last_interact_time += frozenDuration;
    if (ownerPet.daily_feed.last_feed_time > 0)
        ownerPet.daily_feed.last_feed_time += frozenDuration;
}

// 导入宠物时: 将访客/备份宠物的绝对时间戳从导出方时钟对齐到本机时钟
// delta = localNow - donorSaveTime (有符号, 支持本机时钟在前或在后)
// 原理: 导出方存档时 save_time 是其设备时钟, 宠物内所有绝对时间戳也基于该时钟;
//        导入方用 (本机now - 导出方save_time) 作为偏移量, 将宠物时间锚定到本机时钟.
// 安全护栏: |delta| > 10年 视为数据异常, 跳过对齐 (避免溢出/乱标定)
void realignImportedTimestamps(PetState& pet, uint32_t donorSaveTime, uint32_t localNow, uint8_t localDay) {
    if (donorSaveTime == 0) {         // 无锚点, 无法对齐; 仍重置日界
        pet.daily_feed.reset(localDay);
        return;
    }

    int32_t delta = (int32_t)(localNow - donorSaveTime);

    // 安全护栏: |delta| > 10年 (~315M秒) 视为异常, 不做平移
    constexpr int32_t MAX_REASONABLE_DELTA = 315360000;  // 10 * 365 * 24 * 3600
    if (delta > MAX_REASONABLE_DELTA || delta < -MAX_REASONABLE_DELTA) {
        saveio::log.printf("[TimeAlign] WARNING: delta=%ld sec (>10yr), skipping realignment\n", (long)delta);
        // 即使跳过平移, 仍重置日界 (否则 daily_feed 必然错乱)
        pet.daily_feed.reset(localDay);
        return;
    }

    // 若 delta 很小 (< 60秒), 不值得平移, 避免无意义微调
    if (delta > -60 && delta < 60) {
        // 仍对齐日界
        pet.daily_feed.reset(localDay);
        return;
    }

    saveio::log.printf("[TimeAlign] Realigning imported pet timestamps, delta=%ld sec\n", (long)delta);

    // 辅助 lambda: 对非零时间戳施加有符号偏移, 并 clamp 到 [1, localNow] 防止溢出/未来值
    auto shiftTimestamp = [delta, localNow](uint32_t& ts) {
        if (ts == 0) return;  // 0 = 未设置/特殊语义, 保留
        int64_t shifted = (int64_t)ts + delta;
        if (shifted <= 0) shifted = 1;                    // 不允许变为 0 (0 有特殊含义)
        if (shifted > (int64_t)localNow) shifted = localNow;  // 不允许超过当前时间
        ts = (uint32_t)shifted;
    };

    shiftTimestamp(pet.idle_paused_until);
    shiftTimestamp(pet.last_poke_effect_time);
    shiftTimestamp(pet.rhongo_timer_start);
    shiftTimestamp(pet.birth_timestamp);
    shiftTimestamp(pet.last_interact_time);
    shiftTimestamp(pet.daily_feed.last_feed_time);

    // 日界强制对齐到本机当天 (date_day 是日历日, 平移无意义, 直接 reset)
    pet.daily_feed.reset(localDay);
}


namespace save_workflow {
static SaveResult fail(SaveManager& manager,const char* operation,const char* stage,SaveResult result) {
    manager.requireRecovery();
    saveio::log.printf("[%s] FAILED stage=%s error=%u; recovery required; saved records preserved, automatic writes blocked\n",operation,stage,unsigned(result));
    return result;
}
SaveResult persistDevice(SaveManager& manager,const DeviceState& device) {
    const auto result=manager.saveDeviceState(device);
    if(result!=SAVE_OK)fail(manager,"Device","dev_state",result);
    return result;
}
SaveResult restoreOwner(SaveManager& manager,PetState& pet,DeviceState& device,uint32_t now,uint8_t day) {
    if(!device.is_visiting||device.owner_frozen_slot>=SAVE_SLOT_COUNT||manager.recoveryRequired()||manager.operationPending()){
        saveio::log.println("[Visit] Recovery required; visit and owner records preserved.");
        return SAVE_ERR_CORRUPTED;
    }
    auto result=manager.beginOperation(device.owner_frozen_slot);
    if(result!=SAVE_OK)return fail(manager,"Visit","guard",result);
    PetState candidate;
    result=manager.loadFromSlot(device.owner_frozen_slot,candidate);
    if(result!=SAVE_OK)return fail(manager,"Visit","owner_unavailable",result);
    const auto duration=(device.visit_start_epoch==0||now<device.visit_start_epoch)?0:now-device.visit_start_epoch;
    realignOwnerTimestamps(candidate,duration);candidate.daily_feed.reset(day);
    result=manager.setActivePairAfterVisitEnd(device.owner_frozen_slot);
    if(result!=SAVE_OK)return fail(manager,"Visit","slot_cfg",result);
    DeviceState nextDevice=device;
    nextDevice.is_visiting=false;nextDevice.owner_frozen_slot=255;
    nextDevice.visit_start_epoch=0;nextDevice.device_clock_epoch=now;
    result=persistDevice(manager,nextDevice);
    if(result!=SAVE_OK)return fail(manager,"Visit","dev_state",result);
    // Keep the original owner's multi-key v9 record untouched until completion.
    // While the owner stays frozen, save writes the companion slot with a newer
    // sequence. Ordinary two-slot rotation resumes only after finishOperation.
    result=manager.save(candidate,now);
    if(result!=SAVE_OK)return fail(manager,"Visit","final_save",result);
    result=manager.finishOperation();
    if(result!=SAVE_OK)return fail(manager,"Visit","guard_complete",result);
    pet=candidate;device=nextDevice;manager.markSaved(now);
    saveio::log.printf("[Visit] Owner restored. Frozen duration: %lu sec\n",static_cast<unsigned long>(duration));
    return SAVE_OK;
}
SaveResult commitImport(SaveManager& manager,PetState& pet,DeviceState& device,GalleryData& gallery,
                        uint8_t slot,const uint8_t* bytes,size_t size,uint32_t now,uint8_t day) {
    constexpr size_t base=sizeof(SaveHeader)+sizeof(PetState);
    if(!bytes||(size!=base&&size!=base+sizeof(GalleryData))||slot>=SAVE_SLOT_COUNT||
       device.is_visiting||manager.operationPending())return SAVE_ERR_CORRUPTED;
    SaveHeader header;PetState donor;
    memcpy(&header,bytes,sizeof(header));memcpy(&donor,bytes+sizeof(header),sizeof(donor));
    auto result=manager.validateImport(header,donor);
    if(result!=SAVE_OK||header.save_time==0)return result==SAVE_OK?SAVE_ERR_CORRUPTED:result;
    const bool full=size!=base;
    GalleryData nextGallery=gallery;
    if(full){
        for(size_t i=base;i<size;++i)if(bytes[i]>1)return SAVE_ERR_CORRUPTED;
        memcpy(&nextGallery,bytes+base,sizeof(nextGallery));
    }else nextGallery.unlock(donor.form);
    uint8_t owner=255;
    if(!full){
        uint32_t seq=0;
        for(const auto active:{manager.getActiveSlotA(),manager.getActiveSlotB()}){
            SaveHeader saved;SaveResult status;
            if(manager.getSlotStatus(active,saved,status)&&(owner==255||saved.sequence>seq||(saved.sequence==seq&&active<owner))){
                owner=active;seq=saved.sequence;
            }
        }
        if(owner==255||owner==slot)return SAVE_ERR_CORRUPTED;
    }
    result=manager.beginOperation(owner);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","guard",result);
    result=manager.importSlotRaw(slot,header,donor,true);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","pet",result);
    result=manager.saveGallery(nextGallery);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","gallery",result);
    result=full?manager.setActivePairForImportedBaseSlot(slot):manager.setActivePairForVisit(owner);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","slot_cfg",result);
    result=manager.syncGlobalGalleryToSlot(slot);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","slot_gallery",result);
    PetState candidate;
    result=manager.loadFromSlot(slot,candidate);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","load",result);
    realignImportedTimestamps(candidate,header.save_time,now,day);
    DeviceState nextDevice=device;nextDevice.rounds++;nextDevice.device_clock_epoch=now;
    nextDevice.is_visiting=!full;nextDevice.owner_frozen_slot=full?255:owner;
    nextDevice.visit_start_epoch=full?0:now;
    result=persistDevice(manager,nextDevice);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","dev_state",result);
    result=manager.save(candidate,now);
    if(result!=SAVE_OK)return fail(manager,"SaveImport","final_save",result);
    result=manager.finishOperation();
    if(result!=SAVE_OK)return fail(manager,"SaveImport","guard_complete",result);
    pet=candidate;device=nextDevice;gallery=nextGallery;manager.markSaved(now);
    if(full)saveio::log.printf("[SaveImport] Full backup restored to slot=%u, rounds=%u\n",slot,device.rounds);
    else saveio::log.printf("[SaveImport] Visit started: guest in slot=%u, owner frozen in slot=%u, rounds=%u\n",slot,owner,device.rounds);
    return SAVE_OK;
}
}
