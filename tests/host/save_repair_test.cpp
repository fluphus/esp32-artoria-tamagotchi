#include "core/save_workflows.h"
#include "core/save_io.h"
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstddef>
static_assert(sizeof(SaveHeader)==16&&sizeof(PetState)==56&&sizeof(GalleryData)==12&&sizeof(DeviceState)==12,"host test ABI must match checked target fixture layout");
namespace {
int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string(__func__)+":"+std::to_string(__LINE__)+" " #x);}while(false)
struct PowerLoss{};
std::string output;
struct Store {
    std::map<std::string,std::vector<uint8_t>> values;
    bool eraseFail=false;
    std::string fail;int failNth=1,matched=0,cut=0,writes=0;
    std::vector<std::string> written;
    void attach(){saveio::storage={this,
        [](void*,const char*,bool){return true;},
        [](void*p,const char*k){return static_cast<Store*>(p)->values.count(k)!=0;},
        [](void*p,const char*k)->size_t{auto&m=static_cast<Store*>(p)->values;auto i=m.find(k);return i==m.end()?0:i->second.size();},
        [](void*p,const char*k,void*b,size_t n)->size_t{auto&m=static_cast<Store*>(p)->values;auto i=m.find(k);if(i==m.end()||i->second.size()>n)return 0;memcpy(b,i->second.data(),i->second.size());return i->second.size();},
        [](void*p,const char*k,const void*b,size_t n)->size_t{auto&s=*static_cast<Store*>(p);if(s.fail==k&&++s.matched==s.failNth)return 0;auto*d=static_cast<const uint8_t*>(b);s.values[k]={d,d+n};s.written.push_back(k);if(++s.writes==s.cut)throw PowerLoss{};return n;},
        [](void*p){auto&s=*static_cast<Store*>(p);if(s.eraseFail)return false;s.values.clear();return true;}};}
    void track(){fail.clear();failNth=1;matched=0;writes=0;written.clear();cut=0;output.clear();}
};
std::vector<uint8_t> file(const std::string& name){std::ifstream f("tests/fixtures/v9/"+name,std::ios::binary);CHECK(f.good());return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
PetState fresh(){PetState p{};p.initNew(1700000000);return p;}
uint16_t checksum(const std::vector<uint8_t>& bytes){uint16_t sum=0xffff;for(auto v:bytes){sum^=v;sum=uint16_t((sum<<3)|(sum>>13));sum+=v;}return sum;}
template<class T>std::vector<uint8_t> raw(const T& value){const auto*p=reinterpret_cast<const uint8_t*>(&value);return {p,p+sizeof(value)};}
void seed(Store&s,SaveManager&m,PetState&p,DeviceState&d,GalleryData&g,bool visit=false){
    s.attach();CHECK(m.init()==SAVE_OK);p=fresh();p.health=50;
    g.init();g.unlock(p.form);CHECK(m.saveGallery(g)==SAVE_OK);CHECK(m.save(p,1700000000)==SAVE_OK);
    d.init();d.rounds=4;d.device_clock_epoch=1700000000;
    if(visit){p.health=71;CHECK(m.save(p,1700000300)==SAVE_OK);CHECK(m.setActivePair(1,2)==SAVE_OK);p.health=82;CHECK(m.save(p,1700000600)==SAVE_OK);d.is_visiting=true;d.owner_frozen_slot=0;d.visit_start_epoch=1700000000;}
    CHECK(m.saveDeviceState(d)==SAVE_OK);
    CHECK(save_workflow::loadStartup(m,p)==save_workflow::Startup::Loaded);
    CHECK(p.health==(visit?82:50));s.track();
}
void blocked(Store&s,SaveManager&m,PetState&p,DeviceState&d,GalleryData&g){
    CHECK(m.recoveryRequired());const auto retained=s.values;
    CHECK(!m.shouldAutoSave(1700003600));CHECK(m.save(p,1700003600)!=SAVE_OK);
    CHECK(m.saveGallery(g)!=SAVE_OK&&m.saveDeviceState(d)!=SAVE_OK);
    CHECK(m.loadFromSlot(0,p)!=SAVE_OK);CHECK(s.values==retained);
}
void startupAndUnknown(){
    for(const std::string key:{"gallery","dev_state","gallery_v10","device_v10"})for(int kind=0;kind<4;++kind){
        Store s;s.attach();s.values[key]=file(key.find("gallery")!=std::string::npos?"gallery.bin":"device.bin");
        if(kind==1)s.values[key].resize(2);
        if(kind==2)s.values[key][key=="dev_state"?6:0]=2;
        if(kind==3)s.values[key].push_back(0);
        const auto original=s.values[key];SaveManager m;CHECK(m.init()==SAVE_OK&&m.hasSave());
        auto p=fresh();DeviceState d;d.init();GalleryData g;g.init();
        CHECK(save_workflow::loadStartup(m,p)==save_workflow::Startup::Recovery);
        const auto all=s.values;CHECK(!m.shouldAutoSave(1700000000));CHECK(m.save(p,1700000000)!=SAVE_OK);
        CHECK(m.saveGallery(g)!=SAVE_OK&&m.saveDeviceState(d)!=SAVE_OK);CHECK(s.values==all&&s.values[key]==original);
    }
    for(bool newer:{false,true})for(uint8_t bad:{uint8_t(0),uint8_t(1)}){
        Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g);
        CHECK(m.save(p,1700000300)==SAVE_OK);
        const std::string prefix="s"+std::to_string(bad);
        if(newer)s.values[prefix+"_v10"]={1,2,3};else s.values[prefix+"_hdr"][0]=99;
        const auto original=s.values[prefix+"_hdr"];const auto data=s.values[prefix+"_dat"];
        SaveManager boot;CHECK(boot.init()==SAVE_OK);CHECK(save_workflow::loadStartup(boot,p)==save_workflow::Startup::Loaded);
        for(int i=0;i<6;++i){CHECK(boot.save(p,1700000600+i*300)==SAVE_OK);CHECK(s.values[prefix+"_hdr"]==original&&s.values[prefix+"_dat"]==data);if(newer)CHECK(s.values[prefix+"_v10"]==std::vector<uint8_t>({1,2,3}));}
    }
    for(bool thirdBad:{false,true}){
        Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g,true);
        s.values["s0_dat"][0]^=1;if(thirdBad)s.values["s2_dat"][0]^=1;else{s.values.erase("s2_hdr");s.values.erase("s2_dat");s.values.erase("s2_gal");}
        const auto ownerHeader=s.values["s0_hdr"],ownerData=s.values["s0_dat"];
        SaveManager boot;CHECK(boot.init()==SAVE_OK&&boot.load(p)==SAVE_OK&&p.health==71);
        CHECK(boot.getActiveSlotA()!=0&&boot.getActiveSlotB()!=0);
        for(int i=0;i<6;++i){CHECK(boot.save(p,1700000900+i*300)==SAVE_OK);CHECK(s.values["s0_hdr"]==ownerHeader&&s.values["s0_dat"]==ownerData);}
    }
    for(const std::vector<uint8_t>& guard:std::vector<std::vector<uint8_t>>{{},{0xa7},{0xa7,0,255,1,0,0},{0xa7,0,255,99,0,0}}){
        Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g);
        s.values["repair_guard"]=guard;const auto owner=s.values["s0_dat"];
        SaveManager boot;CHECK(boot.init()==SAVE_OK&&boot.hasSave());
        CHECK(save_workflow::loadStartup(boot,p)==save_workflow::Startup::Recovery);
        blocked(s,boot,p,d,g);CHECK(s.values["repair_guard"]==guard&&s.values["s0_dat"]==owner);
    }
    std::cout<<"PASS startup/sidecars, unknown slot retention, frozen owner fallback\n";
}
void workflows(){
    for(bool restore:{false,true})for(bool full:{false,true}){
        if(restore&&full)continue;
        const std::string target=restore?"s1":"s2";
        for(const std::string stage:{"slot_cfg","dev_state","gallery","final_header","final_data","final_gallery","guard_complete","success","owner_bad"}){
            if(restore&&stage=="gallery")continue;
            if(!restore&&stage=="owner_bad")continue;
            Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g,restore);
            if(stage=="owner_bad")s.values["s0_dat"][0]^=1;
            const auto ownerHdr=s.values["s0_hdr"],ownerDat=s.values["s0_dat"],ram=raw(p),dev=raw(d),gal=raw(g);
            auto donor=file(full?"adult-white.full.bin":"adult-white.pet.bin");
            if(stage=="slot_cfg"||stage=="dev_state"||stage=="gallery")s.fail=stage;
            if(stage=="final_header")s.fail=target+"_hdr";
            if(stage=="final_data")s.fail=target+"_dat";
            if(stage=="final_gallery")s.fail=target+"_gal";
            if(stage=="guard_complete"){s.fail="repair_guard";s.failNth=2;}
            const auto result=restore?save_workflow::restoreOwner(m,p,d,1700003600,15):save_workflow::commitImport(m,p,d,g,1,donor.data(),donor.size(),1700003600,15);
            if(stage=="success"){
                CHECK(result==SAVE_OK&&!m.operationPending()&&!m.recoveryRequired());
                CHECK(d.is_visiting==(!restore&&!full));CHECK(p.health==(restore?50:73));
                CHECK(d.rounds==(restore?4:5));CHECK(p.birth_timestamp==(restore?1700003600u:1699744400u));CHECK(p.daily_feed.date_day==15&&p.daily_feed.feed_count==0);CHECK(output.find(restore?"Owner restored":(full?"Full backup restored":"Visit started"))!=std::string::npos);
                for(int i=0;i<4;++i){CHECK(m.save(p,1700003900+i*300)==SAVE_OK);SaveManager boot;CHECK(boot.init()==SAVE_OK&&boot.load(p)==SAVE_OK);CHECK(p.health==(restore?50:73));DeviceState disk;CHECK(boot.loadDeviceState(disk)==SAVE_OK&&disk.is_visiting==d.is_visiting);if(!restore&&!full)CHECK(s.values["s0_hdr"]==ownerHdr&&s.values["s0_dat"]==ownerDat);}
            }else{
                CHECK(result!=SAVE_OK&&m.operationPending());CHECK(output.find("FAILED stage=")!=std::string::npos);
                CHECK(output.find("Owner restored")==std::string::npos&&output.find("Full backup restored")==std::string::npos&&output.find("Visit started")==std::string::npos);
                CHECK(raw(p)==ram&&raw(d)==dev&&raw(g)==gal);
                CHECK(s.values["s0_hdr"]==ownerHdr&&s.values["s0_dat"]==ownerDat);
                blocked(s,m,p,d,g);s.fail.clear();SaveManager boot;CHECK(boot.init()==SAVE_OK);
                CHECK(save_workflow::loadStartup(boot,p)==save_workflow::Startup::Recovery);blocked(s,boot,p,d,g);
                if(stage!="owner_bad"){SaveHeader h;PetState owner;CHECK(boot.exportSlotRaw(0,h,owner)==SAVE_OK&&owner.health==50);}
            }
            DeviceState persisted;CHECK(m.loadDeviceState(persisted)==SAVE_OK);
            const bool deviceWritten=stage=="success"||stage=="final_header"||stage=="final_data"||stage=="final_gallery"||stage=="guard_complete";
            CHECK(persisted.is_visiting==(deviceWritten?(!restore&&!full):restore));
            std::cout<<"NVS visiting="<<persisted.is_visiting<<" pending="<<m.operationPending()<<" owner_preserved="<<(s.values["s0_hdr"]==ownerHdr&&s.values["s0_dat"]==ownerDat)<<'\n';
            if(stage!="success"){
                const auto pos=output.find(restore?"[Visit] FAILED":"[SaveImport] FAILED");
                std::cout<<"SERIAL "<<output.substr(pos,output.find('\n',pos)-pos)<<'\n';
            }
            if(stage=="final_data"){
                SaveHeader header;PetState owner;GalleryData recoveredGallery;bool bound=false;
                CHECK(m.exportSlotRawWithGallery(0,header,owner,recoveredGallery,bound)==SAVE_OK);
                auto backup=raw(header);const auto ownerBytes=raw(owner),galleryBytes=raw(recoveredGallery);
                backup.insert(backup.end(),ownerBytes.begin(),ownerBytes.end());
                backup.insert(backup.end(),galleryBytes.begin(),galleryBytes.end());
                s.eraseFail=true;const auto retained=s.values;
                CHECK(m.erase()==SAVE_ERR_WRITE&&s.values==retained&&m.operationPending());s.eraseFail=false;
                CHECK(m.erase()==SAVE_OK); // Simulates the existing explicit erase command only.
                SaveManager restart;CHECK(restart.init()==SAVE_OK);
                PetState recovered;CHECK(save_workflow::loadStartup(restart,recovered)==save_workflow::Startup::NewGame);
                DeviceState emptyDevice;emptyDevice.init();GalleryData emptyGallery;emptyGallery.init();
                CHECK(save_workflow::commitImport(restart,recovered,emptyDevice,emptyGallery,0,backup.data(),backup.size(),1700007200,15)==SAVE_OK);
                SaveManager finalBoot;CHECK(finalBoot.init()==SAVE_OK&&finalBoot.load(recovered)==SAVE_OK&&recovered.health==50);
                CHECK(finalBoot.loadDeviceState(emptyDevice)==SAVE_OK&&!emptyDevice.is_visiting);
            }
            std::cout<<"RESULT "<<(restore?"restore":(full?"full_import":"visit_import"))<<' '<<stage<<" result="<<unsigned(result)<<" RAM_HP="<<p.health<<" visiting="<<d.is_visiting<<" reboot="<<(stage=="success"?"Loaded":"Recovery")<<'\n';
        }
        const std::vector<std::string> steps=restore?std::vector<std::string>{"repair_guard","slot_cfg","dev_state","s1_hdr","s1_dat","s1_gal","repair_guard"}:std::vector<std::string>{"repair_guard","s1_hdr","s1_dat","gallery","slot_cfg","s1_gal","dev_state","s2_hdr","s2_dat","s2_gal","repair_guard"};
        for(size_t cut=1;cut<=steps.size();++cut){
            Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g,restore);
            const auto ownerHdr=s.values["s0_hdr"],ownerDat=s.values["s0_dat"],ram=raw(p),dev=raw(d),gal=raw(g);
            auto donor=file(full?"adult-white.full.bin":"adult-white.pet.bin");s.cut=int(cut);bool interrupted=false;
            try{if(restore)save_workflow::restoreOwner(m,p,d,1700003600,15);else save_workflow::commitImport(m,p,d,g,1,donor.data(),donor.size(),1700003600,15);}catch(const PowerLoss&){interrupted=true;}
            CHECK(interrupted&&s.writes==int(cut)&&s.written.back()==steps[cut-1]);
            CHECK(raw(p)==ram&&raw(d)==dev&&raw(g)==gal);
            CHECK(output.find("Owner restored")==std::string::npos&&output.find("Full backup restored")==std::string::npos&&output.find("Visit started")==std::string::npos);
            CHECK(s.values["s0_hdr"]==ownerHdr&&s.values["s0_dat"]==ownerDat);
            s.cut=0;SaveManager boot;CHECK(boot.init()==SAVE_OK);
            if(cut<steps.size()){CHECK(save_workflow::loadStartup(boot,p)==save_workflow::Startup::Recovery);blocked(s,boot,p,d,g);}
            else{CHECK(save_workflow::loadStartup(boot,p)==save_workflow::Startup::Loaded);CHECK(p.health==(restore?50:73));CHECK(boot.loadDeviceState(d)==SAVE_OK&&d.is_visiting==(!restore&&!full));CHECK(boot.save(p,1700003900)==SAVE_OK);SaveManager again;CHECK(again.init()==SAVE_OK&&again.load(p)==SAVE_OK&&p.health==(restore?50:73));}
            std::cout<<"POWER_CUT "<<(restore?"restore":(full?"full_import":"visit_import"))<<" after="<<cut<<':'<<steps[cut-1]<<" reboot="<<(cut<steps.size()?"Recovery":"Loaded")<<'\n';
        }
    }
}
void fixtures(){
    for(const std::string name:{"adult-white.pet.bin","adult-white.full.bin","black.pet.bin","rhongo.pet.bin","nobu.pet.bin"}){
        Store s;s.attach();SaveManager m;CHECK(m.init()==SAVE_OK);auto data=file(name);
        s.values["s0_hdr"]={data.begin(),data.begin()+16};s.values["s0_dat"]={data.begin()+16,data.begin()+72};
        if(data.size()==84)s.values["s0_gal"]={data.begin()+72,data.end()};
        PetState p;CHECK(m.load(p)==SAVE_OK);SaveHeader h;PetState exported;GalleryData g;bool bound;
        CHECK(m.exportSlotRawWithGallery(0,h,exported,g,bound)==SAVE_OK);
        CHECK(raw(h)==std::vector<uint8_t>(data.begin(),data.begin()+16));CHECK(raw(exported)==std::vector<uint8_t>(data.begin()+16,data.begin()+72));
        if(data.size()==84)CHECK(raw(g)==std::vector<uint8_t>(data.begin()+72,data.end()));
    }
    Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g);
    auto bad=file("adult-white.full.bin");
    for(int kind=0;kind<5;++kind){auto bytes=bad;if(kind==0)bytes.resize(71);if(kind==1)bytes[0]=99;if(kind==2)bytes[20]^=1;if(kind==3){bytes[16]=99;std::vector<uint8_t> pet(bytes.begin()+16,bytes.begin()+72);auto sum=checksum(pet);bytes[4]=uint8_t(sum);bytes[5]=uint8_t(sum>>8);}if(kind==4)bytes[72]=2;const auto before=s.values;CHECK(save_workflow::commitImport(m,p,d,g,1,bytes.data(),bytes.size(),1700003600,15)!=SAVE_OK);CHECK(s.values==before&&!m.operationPending());}
    std::cout<<"PASS target v9 fixture read/export and invalid import refusal\n";
}
}
int main(){try{saveio::diagnostic=[](const char*s){output+=s;};fixtures();startupAndUnknown();workflows();std::cout<<"PASS original production save repair: "<<checks<<" checks\n";}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
