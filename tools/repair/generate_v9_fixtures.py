#!/usr/bin/env python3
"""Extract original v9 objects with ESP32-S3 compiler, never host struct dumping."""
import hashlib, json, os, struct, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE='448dbe158595c1db9f1d2fc37caa15810c53de1c'
OUT=ROOT/'tests/fixtures/v9'
PREFIX=Path(os.environ.get('XTENSA_PREFIX', str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-')))
fields=['form','base_form','alignment','stage','health','seriousness','idle_minute_remainder','idle_paused_until','last_poke_effect_time','rhongo_timer_start','is_rhongomyniad','white_fun_form','white_fun_form_locked','mapo_tofu_count','is_black_rhongomyniad','is_nobu','is_oda_nobunaga','birth_timestamp','last_interact_time','age_days','daily_feed']
headers=['version','data_size','checksum','sequence','save_time']
def run(args): return subprocess.check_output([str(x) for x in args])
def checksum(data):
    value=0xffff
    for byte in data:
        value^=byte
        value=((value<<3)|(value>>13))&0xffff
        value=(value+byte)&0xffff
    return value
with tempfile.TemporaryDirectory(prefix='esp32-v9-') as directory:
    temp=Path(directory)
    header=run(['git','-C',ROOT,'show',f'{BASE}:src/core/game_state.h'])
    (temp/'original_game_state.h').write_bytes(header)
    (temp/'core').mkdir();(temp/'pet').mkdir()
    (temp/'core/game_state.h').write_bytes(header)
    (temp/'pet/gallery.h').write_bytes(run(['git','-C',ROOT,'show',f'{BASE}:src/pet/gallery.h']))
    offset=', '.join('offsetof(PetState, '+f+')' for f in fields)
    hoffset=', '.join('offsetof(SaveHeader, '+f+')' for f in headers)
    source='''#include "original_game_state.h"
#include "pet/gallery.h"
#include <stddef.h>
extern "C" {
__attribute__((section(".fixture.layout"),used)) const uint32_t layout[] = {
sizeof(PetState), sizeof(SaveHeader), sizeof(DeviceState), sizeof(DailyFeedState), OFFSET, HOFFSET, sizeof(GalleryData), offsetof(DeviceState,device_clock_epoch),offsetof(DeviceState,rounds),offsetof(DeviceState,is_visiting),offsetof(DeviceState,owner_frozen_slot),offsetof(DeviceState,visit_start_epoch)};
__attribute__((section(".fixture.pet"),used)) const PetState fixture = {
FORM_WHITE_LANCER, FORM_WHITE_SABER, ALIGN_WHITE, STAGE_ADULT,
73, 100, 4, 1700001800, 1700000000, 1699900000, false,
FORM_WHITE_ARCHER, true, 2, false, false, false, 1699740800, 1700000000, 3,
{14, 2, 1699998200, 1, 0, 0, 1}};
__attribute__((section(".fixture.header"),used)) const SaveHeader header = {9, sizeof(PetState), 0, 42, 1700000000};
}
'''.replace('HOFFSET',hoffset).replace('OFFSET',offset)
    source=source.replace('const PetState fixture =', 'constexpr PetState fixture =')
    source+='\nconstexpr PetState variant(int kind) {\n    PetState p=fixture;\n    if(kind==1){p.form=FORM_NOBU;p.base_form=FORM_NOBU;p.alignment=ALIGN_UNDETERMINED;p.stage=STAGE_CHILD;p.is_nobu=true;p.mapo_tofu_count=0;}\n    if(kind==2){p.form=FORM_BLACK_LANCER_RHONGOMYNIAD;p.alignment=ALIGN_BLACK;p.is_black_rhongomyniad=true;p.mapo_tofu_count=3;}\n    if(kind==3){p.form=FORM_WHITE_LANCER_RHONGOMYNIAD;p.is_rhongomyniad=true;}\n    return p;\n}\n__attribute__((section(".fixture.nobu"),used)) const PetState nobu=variant(1);\n__attribute__((section(".fixture.black"),used)) const PetState black=variant(2);\n__attribute__((section(".fixture.rhongo"),used)) const PetState rhongo=variant(3);\n'
    source+='\n__attribute__((section(".fixture.gallery"),used)) const GalleryData gallery={{true,true,false,true,false,true,false,false,false,false,false,false}};\n__attribute__((section(".fixture.device"),used)) const DeviceState device={1700000000,7,false,255,0};\n__attribute__((section(".fixture.device_visit"),used)) const DeviceState device_visit={1700000000,7,true,0,1700000000};\n'
    (temp/'fixture.cpp').write_text(source)
    compile_command=[str(PREFIX)+'g++','-std=gnu++17','-c',str(temp/'fixture.cpp'),'-o',str(temp/'fixture.o')]
    run(compile_command)
    data={}
    for section in ('layout','pet','header','nobu','black','rhongo','gallery','device','device_visit'):
        target=temp/(section+'.bin')
        run([str(PREFIX)+'objcopy','--dump-section',f'.fixture.{section}={target}',temp/'fixture.o'])
        data[section]=target.read_bytes()
    values=struct.unpack('<'+'I'*(len(data['layout'])//4),data['layout'])
    layout={'pet_size':values[0],'header_size':values[1],'device_size':values[2],'daily_size':values[3], 'pet_offsets':dict(zip(fields,values[4:4+len(fields)])), 'header_offsets':dict(zip(headers,values[4+len(fields):]))}
    hdr=bytearray(data['header'])
    struct.pack_into('<H',hdr,layout['header_offsets']['checksum'],checksum(data['pet']))
    OUT.mkdir(parents=True,exist_ok=True)
    payload=bytes(hdr)+data['pet']
    gallery=data['gallery']
    layout['gallery_size']=values[30]
    layout['device_offsets']=dict(zip(['device_clock_epoch','rounds','is_visiting','owner_frozen_slot','visit_start_epoch'],values[31:]))
    for name in ('gallery','device','device_visit'):(OUT/(name+'.bin')).write_bytes(data[name])
    for name,raw in [('adult-white.pet.bin',payload),('adult-white.full.bin',payload+gallery)]:
        (OUT/name).write_bytes(raw)
    for name in ('nobu','black','rhongo'):
        variantHeader=bytearray(data['header'])
        struct.pack_into('<H',variantHeader,layout['header_offsets']['checksum'],checksum(data[name]))
        (OUT/(name+'.pet.bin')).write_bytes(variantHeader+data[name])
    manifest={'source_commit' :BASE,'source_header_sha256':hashlib.sha256(header).hexdigest(),'compiler':run([str(PREFIX)+'g++','--version']).decode().splitlines()[0], 'compile_flags':['-std=gnu++17','-c'], 'layout':layout,'fixtures':{name:hashlib.sha256((OUT/name).read_bytes()).hexdigest() for name in ['adult-white.pet.bin','adult-white.full.bin','nobu.pet.bin','black.pet.bin','rhongo.pet.bin','gallery.bin','device.bin','device_visit.bin']},'origin':'Static aggregate objects compiled with original firmware header and target Xtensa toolchain; checksum filled over target pet bytes using original algorithm. Gallery and DeviceState objects also use original headers and target compiler. No hardware dump; runtime padding and real NVS failures not sampled.'}
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))
