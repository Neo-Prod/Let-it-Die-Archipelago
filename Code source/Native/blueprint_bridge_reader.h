#ifndef LID_BLUEPRINT_BRIDGE_READER_H
#define LID_BLUEPRINT_BRIDGE_READER_H
#include "blueprint_wire.h"
/* Candidate fixed-file reader; include progression_bridge_io.h and catalogue.
   One owner thread, same as character bridge. Called before engine locks only.
   No polling here and no save-file API. Not wired into mod.c. */
#define LID_BLUEPRINT_BRIDGE_SIZE (320u+4u*LID_BLUEPRINT_FAMILY_COUNT)
typedef struct LidBlueprintBridgeReader {
    LidBridgeIo* bridge;
    BYTE scratch[LID_BLUEPRINT_BRIDGE_SIZE+1];
} LidBlueprintBridgeReader;
static int lid_blueprint_bridge_read(void* context,BYTE* out,size_t capacity,size_t* size){
    static const wchar_t name[]=L"blueprint_target.bin";
    LidBlueprintBridgeReader* reader=context;LidBridgeIo* io;size_t n=0,i;
    HANDLE file;DWORD got=0,error;BOOL read_ok,close_ok;
    if(size)*size=0;
    if(!reader||!(io=reader->bridge)||!io->initialized||!out||!size||capacity!=LID_BLUEPRINT_BRIDGE_SIZE||
       !io->ops.open||!io->ops.read||!io->ops.close||!io->ops.error)return -1;
    while(n<1000&&io->directory[n]){io->path[n]=io->directory[n];n++;}
    if(!n||n>=1000)return -1;
    for(i=0;name[i];i++)io->path[n++]=name[i];io->path[n]=0;
    file=io->ops.open(io->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE){error=io->ops.error();return error==2||error==3?0:-1;}
    read_ok=io->ops.read(file,reader->scratch,sizeof(reader->scratch),&got,0);
    close_ok=io->ops.close(file);
    if(!read_ok||!close_ok||got!=LID_BLUEPRINT_BRIDGE_SIZE)return -1;
    lid_wire_copy(out,reader->scratch,LID_BLUEPRINT_BRIDGE_SIZE);*size=LID_BLUEPRINT_BRIDGE_SIZE;return 1;
}
/* Publish only an already-proved SAVED frame, after engine reservations close.
   This validates the frame, NOT its authority: transaction owns save/readback
   proof and exact frozen-target binding. Never call from a receipt handler. */
static int lid_blueprint_bridge_publish_saved(LidBlueprintBridgeReader* reader,const BYTE* data,size_t size){
    static const wchar_t name[]=L"blueprint_native_saved.bin";
    LidBridgeIo* io;LidWireMessage saved,original,decoded;BYTE target[256];unsigned counts[329];
    size_t n=0,i;HANDLE file;DWORD wrote=0;BOOL ok,closed;
    if(!reader||!(io=reader->bridge)||!io->initialized||!data||size!=LID_BLUEPRINT_BRIDGE_SIZE||
       !io->ops.open||!io->ops.write||!io->ops.flush||!io->ops.close||!io->ops.replace||!io->ops.remove||
       !lid_wire_decode(&saved,data+64,256)||saved.kind!=LID_WIRE_SAVED)return 0;
    original=saved;original.kind=LID_WIRE_TARGET;original.status=0;
    if(!lid_wire_encode(target,256,&original)||
       !lid_blueprint_saved_decode(counts,329,&decoded,data,size,target,lid_blueprint_catalogue_hash))return 0;
    while(n<1000&&io->directory[n]){io->path[n]=io->directory[n];n++;}
    if(!n||n>=1000)return 0;
    for(i=0;name[i];i++)io->path[n++]=name[i];io->path[n]=0;
    for(i=0;i<n;i++)io->temporary[i]=io->path[i];
    for(i=0;i<40&&io->suffix[i];i++)io->temporary[n++]=io->suffix[i];
    if(!i||i==40)return 0;io->temporary[n]=0;
    file=io->ops.open(io->temporary,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(file==INVALID_HANDLE_VALUE)return 0;
    ok=io->ops.write(file,data,(DWORD)size,&wrote,0);
    if(ok&&wrote==size)ok=io->ops.flush(file);else ok=0;
    closed=io->ops.close(file);
    if(!ok||!closed||!io->ops.replace(io->temporary,io->path,9)){
        io->ops.remove(io->temporary);return 0;
    }
    return 1;
}
#endif
