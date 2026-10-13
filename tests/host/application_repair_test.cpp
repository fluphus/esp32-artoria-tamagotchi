// Actual original main and callbacks, not a reimplementation of the app.
#define main existing_storage_test_main
#include "save_repair_test.cpp"
#undef main
#include "../../src/main.cpp"
#include <chrono>
uint32_t hostMillis=0,hostRandom=999;int hostPins[64];HostSerial Serial;
// No hardware power/sleep tests: only replace unrelated power notifications.
PowerManager powerManager;
void PowerManager::init(){}
void PowerManager::update(uint32_t){}
void PowerManager::onUserActivity(){}
void PowerManager::setBrightness(uint8_t){}
void PowerManager::setDimBrightness(uint8_t){}
void PowerManager::setDimTimeout(uint32_t){}
void PowerManager::setOffTimeout(uint32_t){}
void PowerManager::saveConfig(){}
void PowerManager::scheduleDeepSleepRetry(uint32_t,uint32_t){}
void PowerManager::cancelDeepSleepRetry(){}
bool PowerManager::isDeepSleepRetryDue(uint32_t) const{return false;}
void PowerManager::onWakeFromSleep(){}
void frame(uint32_t ms){hostMillis+=ms;menuController.update();DisplayManager::update(hostMillis);DisplayManager::renderIfDirty();}
void click(ButtonId id){hostPins[BTN_PIN_MAP[id]]=LOW;frame(31);hostPins[BTN_PIN_MAP[id]]=HIGH;frame(31);}
Store store;
void prepare(){
 store=Store{};store.attach();saveio::diagnostic=[](const char*s){output+=s;Serial.print(s);};
 hostMillis=0;for(auto& pin:hostPins)pin=HIGH;Serial.output.clear();Serial.input.clear();
 saveManager=SaveManager{};CHECK(saveManager.init()==SAVE_OK);
 timeManager.init();timeManager.setSimulatedTime(2024,1,1,8,0);
 gallerySystem.init();pet.initNew(timeManager.now());deviceState.init();
 gallerySystem.unlockForm(pet.form);CHECK(saveManager.saveGallery(gallerySystem.getData())==SAVE_OK);
 CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK);deviceState.device_clock_epoch=timeManager.now();
 CHECK(saveManager.saveDeviceState(deviceState)==SAVE_OK);
 CHECK(save_workflow::loadStartup(saveManager,pet)==save_workflow::Startup::Loaded);
 menuController.init(&pet,&deviceState,&gameCallbacks);DisplayManager::init();frame(5000);
 waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;g_slotImport={};cmdLen=0;cmdDiscard=false;
 store.track();Serial.output.clear();
}
void clockRollover(){prepare();auto previous=timeManager.now();for(int i=0;i<50;++i){hostMillis+=100000000;auto now=timeManager.now();CHECK(now==previous+100000);previous=now;}}
void clockFraction(){prepare();auto before=timeManager.now();hostMillis+=999;CHECK(timeManager.now()==before);hostMillis+=1;CHECK(timeManager.now()==before+1);}
void destroyRelease(){prepare();waitingForTimeSet=false;
 auto chord=[](){for(auto pin:BTN_PIN_MAP)hostPins[pin]=LOW;frame(31);frame(BTN_DESTROY_HOLD_MS+1);for(auto pin:BTN_PIN_MAP)hostPins[pin]=HIGH;frame(31);};
 auto rounds=deviceState.rounds;auto disk=store.values;chord();
 CHECK(menuController.getCurrentContext()==UI_DESTROY_CONFIRM&&deviceState.rounds==rounds&&store.values==disk);
 click(BTN_M);CHECK(deviceState.rounds==rounds&&menuController.getCurrentContext()==UI_IDLE);
 chord();CHECK(menuController.getCurrentContext()==UI_DESTROY_CONFIRM);click(BTN_L);click(BTN_M);
 CHECK(deviceState.rounds==rounds+1);disk=store.values;frame(10000);CHECK(store.values==disk);
}
void displayRollover(){prepare();hostMillis=UINT32_MAX-1000;DisplayManager::showFeedResult(FeedOutcome{}, pet.seriousness);hostMillis+=100;DisplayManager::update(hostMillis);
 CHECK(DisplayManager::getCurrentPage()==PAGE_FEED_RESULT);hostMillis+=PAGE_DURATION_FEED_RESULT;DisplayManager::update(hostMillis);CHECK(DisplayManager::getCurrentPage()==PAGE_IDLE);
 hostMillis=UINT32_MAX-100;DisplayManager::showToast("wrap",1000);hostMillis+=50;DisplayManager::update(hostMillis);CHECK(DisplayManager::getModel().toast[0]);
 hostMillis+=950;DisplayManager::update(hostMillis);CHECK(!DisplayManager::getModel().toast[0]);
}
void initialTimeFailures(){
 for(const std::string fault:{"repair_guard","gallery","dev_state","s1_hdr","s1_dat","s1_gal"}){
  prepare();const auto before=pet;const auto device=deviceState;const auto owner=store.values["s0_dat"];
  store.fail=fault;gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);
  CHECK(waitingForTimeSet&&saveManager.recoveryRequired());CHECK(memcmp(&pet,&before,sizeof(pet))==0&&memcmp(&deviceState,&device,sizeof(device))==0);
  CHECK(Serial.output.find("entering normal operation")==std::string::npos&&store.values["s0_dat"]==owner);
  store.fail.clear();SaveManager boot;CHECK(boot.init()==SAVE_OK);
  CHECK(save_workflow::loadStartup(boot,pet)==(fault=="repair_guard"?save_workflow::Startup::Loaded:save_workflow::Startup::Recovery));
 }
 prepare();store.fail="repair_guard";store.failNth=2;gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);
 CHECK(waitingForTimeSet&&saveManager.operationPending());CHECK(Serial.output.find("entering normal operation")==std::string::npos);
 prepare();gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);CHECK(!waitingForTimeSet&&!saveManager.operationPending());
 SaveManager boot;CHECK(boot.init()==SAVE_OK&&save_workflow::loadStartup(boot,pet)==save_workflow::Startup::Loaded);
}
void capacity(){prepare();pet.age_days=UINT16_MAX;const auto before=pet;const auto disk=store.values;doDayEnd();CHECK(memcmp(&before,&pet,sizeof(pet))==0&&store.values==disk);
 prepare();const auto beforeOffline=pet;const auto beforeDisk=store.values;skipTime(367u*86400);CHECK(memcmp(&beforeOffline,&pet,sizeof(pet))==0&&store.values==beforeDisk);
}
void offlineLimit(){prepare();const auto start=timeManager.now();const auto clock=std::chrono::steady_clock::now();
 CHECK(skipTime(MAX_OFFLINE_SECONDS));CHECK(pet.age_days==366&&timeManager.now()==start+MAX_OFFLINE_SECONDS);
 const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-clock).count();
 std::cout<<"MAX_OFFLINE 366 days host_ms="<<ms<<'\n';
 prepare();pet.age_days=UINT16_MAX-1;const auto before=store.values;CHECK(!skipTime(2u*86400)&&store.values==before);
}
void lineProtocol(){prepare();waitingForTimeSet=false;
 Serial.input="SAVE_IMPORT_BEGIN 1\n";readSerialCommand();CHECK(g_slotImport.active);
 Serial.input="SAVE_IMPORT_DATA AA\n";readSerialCommand();CHECK(g_slotImport.received==1);
 hostMillis+=30001;readSerialCommand();CHECK(!g_slotImport.active);
 prepare();Serial.input="SAVE_IMPORT_BEGIN 1"+std::string(90,' ')+"\n";readSerialCommand();CHECK(!g_slotImport.active);
 prepare();Serial.input="SAVE_IMPORT_";readSerialCommand();hostMillis+=30001;readSerialCommand();Serial.input="BEGIN 1\n";readSerialCommand();CHECK(!g_slotImport.active);
}

void storageBoundaries(){
 prepare();SaveHeader header;memcpy(&header,store.values["s0_hdr"].data(),sizeof(header));
 header.sequence=UINT32_MAX-1;store.values["s0_hdr"]=raw(header);
 SaveManager m;CHECK(m.init()==SAVE_OK&&save_workflow::loadStartup(m,pet)==save_workflow::Startup::Loaded);
 pet.health=73;CHECK(m.save(pet,timeManager.now())==SAVE_OK);const auto retained=store.values;
 pet.health=81;CHECK(m.save(pet,timeManager.now())!=SAVE_OK&&!m.shouldAutoSave(timeManager.now()));CHECK(store.values==retained);
 CHECK(m.importSlotRaw(0,header,pet,true)!=SAVE_OK&&store.values==retained);
 SaveManager reboot;CHECK(reboot.init()==SAVE_OK&&save_workflow::loadStartup(reboot,pet)==save_workflow::Startup::Loaded&&pet.health==73);
 prepare();const auto before=store.values;saveio::storage.open=[](void*,const char*,bool){return false;};
 ::setup();CHECK(Serial.output.find("[Main] New game")==std::string::npos);
 CHECK(saveManager.recoveryRequired()&&!saveManager.shouldAutoSave(timeManager.now()));
 gameCallbacks.onInitialTimeConfirm(2024,1,2,8,0);loop();CHECK(store.values==before);
 CHECK(Serial.output.find("entering normal operation")==std::string::npos);
}
void timeInterruptions(){
 for(int cut=1;cut<=7;++cut){
  prepare();store.cut=cut;bool interrupted=false;
  try{gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);}catch(const PowerLoss&){interrupted=true;}
  CHECK(interrupted);store.cut=0;SaveManager boot;CHECK(boot.init()==SAVE_OK);
  CHECK(save_workflow::loadStartup(boot,pet)==(cut<7?save_workflow::Startup::Recovery:save_workflow::Startup::Loaded));
  if(cut<7){const auto before=store.values;CHECK(boot.save(pet,1700000000)!=SAVE_OK&&store.values==before);}
 }
 prepare();store.fail="dev_state";processCommand("SET_TIME 1704096000");
 CHECK(waitingForTimeSet&&saveManager.recoveryRequired()&&Serial.output.find("entering normal operation")==std::string::npos);
}
void normalSetTime(){prepare();waitingForTimeSet=false;store.fail="dev_state";
 const auto before=pet;const auto device=deviceState;const auto clock=timeManager.now();
 processCommand("SET_TIME 1704096005");CHECK(saveManager.recoveryRequired()&&memcmp(&pet,&before,sizeof(pet))==0&&memcmp(&deviceState,&device,sizeof(device))==0&&timeManager.now()==clock);
 CHECK(Serial.output.find("System time saved")==std::string::npos);
 prepare();waitingForTimeSet=false;processCommand("SET_TIME 1704096005");CHECK(deviceState.device_clock_epoch==1704096005&&timeManager.now()==1704096000);
}
void protocolCompletion(){
 prepare();const auto payload=file("adult-white.full.bin");
 Serial.input="SAVE_IMPORT_BEGIN 1\n";readSerialCommand();CHECK(g_slotImport.active);
 for(size_t offset=0;offset<payload.size();offset+=16){
  char buffer[64];const auto n=snprintf(buffer,sizeof(buffer),"SAVE_IMPORT_DATA @%u ",unsigned(offset));
  size_t size=std::min(size_t(16),payload.size()-offset);
  for(size_t i=0;i<size;++i)snprintf(buffer+n+i*2,3,"%02X",payload[offset+i]);
  const std::string command=std::string(buffer)+"\n";
  Serial.input=command.substr(0,10);readSerialCommand();CHECK(g_slotImport.received==offset);
  Serial.input=command.substr(10);readSerialCommand();CHECK(g_slotImport.received==offset+size);
  Serial.input=command;readSerialCommand();CHECK(g_slotImport.received==offset+size);
 }
 auto rounds=deviceState.rounds;Serial.input="SAVE_IMPORT_COMMIT\n";readSerialCommand();
 CHECK(!g_slotImport.active&&deviceState.rounds==rounds+1&&!saveManager.operationPending());
 const auto saved=store.values;Serial.input="SAVE_IMPORT_COMMIT\n";readSerialCommand();CHECK(store.values==saved&&deviceState.rounds==rounds+1);
 for(const auto ending:{"SAVE_IMPORT_DATA ZZ\n","SAVE_IMPORT_COMMIT\n","SAVE_IMPORT_ABORT\n"}){
  prepare();const auto before=store.values;Serial.input="SAVE_IMPORT_BEGIN 1\nSAVE_IMPORT_DATA AA\n";readSerialCommand();
  Serial.input=ending;readSerialCommand();CHECK(!g_slotImport.active&&store.values==before);
 }
}

void firstTimeSetup(){
 for(const std::string fault:{"", "repair_guard","gallery","dev_state","s0_hdr","s0_dat","s0_gal"}){
  prepare();store.values.clear();saveManager=SaveManager{};::setup();CHECK(waitingForTimeSet&&reinitPetAfterInitialTimeConfirm);
  const auto before=pet;store.track();store.fail=fault;
  gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);
  if(fault.empty()){CHECK(!waitingForTimeSet&&!reinitPetAfterInitialTimeConfirm&&pet.birth_timestamp==timeManager.now());}
  else{CHECK(waitingForTimeSet&&reinitPetAfterInitialTimeConfirm&&saveManager.recoveryRequired()&&memcmp(&pet,&before,sizeof(pet))==0);
   CHECK(Serial.output.find("entering normal operation")==std::string::npos);}
 }
}
void visitTimeFailure(){
 prepare();store=Store{};saveManager=SaveManager{};seed(store,saveManager,pet,deviceState,gallerySystem.getData(),true);
 timeManager.setSimulatedTime(2024,1,1,8,0);deviceState.device_clock_epoch=timeManager.now();deviceState.visit_start_epoch=timeManager.now();
 CHECK(saveManager.saveDeviceState(deviceState)==SAVE_OK);
 const auto owner=store.values["s0_dat"];const auto before=pet;const auto device=deviceState;store.fail="dev_state";
 gameCallbacks.onInitialTimeConfirm(2024,1,2,8,0);
 CHECK(waitingForTimeSet&&saveManager.recoveryRequired()&&deviceState.is_visiting);
 CHECK(store.values["s0_dat"]==owner&&memcmp(&pet,&before,sizeof(pet))==0&&memcmp(&deviceState,&device,sizeof(device))==0);
 const auto locked=store.values;hostMillis+=120000;Serial.input="btn l\n";loop();CHECK(store.values==locked&&memcmp(&pet,&before,sizeof(pet))==0);
 store.fail.clear();SaveManager boot;CHECK(boot.init()==SAVE_OK&&save_workflow::loadStartup(boot,pet)==save_workflow::Startup::Recovery);
}
void manualRecovery(){
 prepare();store.fail="s1_dat";gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);CHECK(saveManager.recoveryRequired());
 store.fail.clear();Serial.output.clear();processCommand("SAVE_EXPORT 0");
 std::vector<uint8_t> payload;size_t offset=0;
 while((offset=Serial.output.find("SAVE_EXPORT_DATA ",offset))!=std::string::npos){
  offset+=17;const auto end=Serial.output.find('\n',offset);const auto hex=Serial.output.substr(offset,end-offset);
  uint8_t bytes[16];size_t size=0;CHECK(decodeHexToBytes(hex.c_str(),bytes,sizeof(bytes),&size));payload.insert(payload.end(),bytes,bytes+size);offset=end;
 }
 CHECK(payload.size()==84);const auto protectedDisk=store.values;loop();CHECK(store.values==protectedDisk);
 processCommand("erase");CHECK(!saveManager.recoveryRequired()&&!deviceState.is_visiting);
 processCommand("SAVE_IMPORT_BEGIN 1");
 for(size_t i=0;i<payload.size();i+=16){char line[64];const int prefix=snprintf(line,sizeof(line),"SAVE_IMPORT_DATA @%u ",unsigned(i));
  const auto size=std::min(size_t(16),payload.size()-i);for(size_t j=0;j<size;++j)snprintf(line+prefix+2*j,3,"%02X",payload[i+j]);processCommand(line);}
 processCommand("SAVE_IMPORT_COMMIT");CHECK(!waitingForTimeSet&&pet.health==50&&!saveManager.operationPending());
 SaveManager boot;CHECK(boot.init()==SAVE_OK&&save_workflow::loadStartup(boot,pet)==save_workflow::Startup::Loaded&&pet.health==50);
}
int main(int argc,char** argv){
 if(argc>1&&std::string(argv[1])=="--bridge"){
  prepare();Serial.output.clear();std::string line;
  while(std::getline(std::cin,line)){Serial.input=line+"\n";readSerialCommand();std::cout<<Serial.output<<std::flush;Serial.output.clear();}return 0;
 }
 int failures=0;for(auto test:{std::pair<const char*,void(*)()>{"clockRollover",clockRollover},{"clockFraction",clockFraction},{"destroyRelease",destroyRelease},{"displayRollover",displayRollover},{"initialTimeFailures",initialTimeFailures},{"capacity",capacity},{"offlineLimit",offlineLimit},{"lineProtocol",lineProtocol},{"storageBoundaries",storageBoundaries},{"timeInterruptions",timeInterruptions},{"normalSetTime",normalSetTime},{"protocolCompletion",protocolCompletion},{"firstTimeSetup",firstTimeSetup},{"visitTimeFailure",visitTimeFailure},{"manualRecovery",manualRecovery}}){try{test.second();std::cout<<"PASS "<<test.first<<'\n';}catch(const std::exception&e){++failures;std::cerr<<"FAIL "<<test.first<<" "<<e.what()<<'\n';}}
 std::cout<<checks<<" application assertions, failures="<<failures<<'\n';return failures?1:0;}
