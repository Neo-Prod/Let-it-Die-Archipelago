#ifndef LID_PROGRESSION_BRIDGE_IO_H
#define LID_PROGRESSION_BRIDGE_IO_H
/* Fixed bridge files only; no arbitrary filename or save-file API.
   Include progression_host.h first. One UI-thread owner per context.
   Buffers live in context, not >4KB automatic stack frames. */
typedef struct LidBridgeIoOps {
    HANDLE (WINAPI *open)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
    BOOL (WINAPI *read)(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
    BOOL (WINAPI *write)(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
    BOOL (WINAPI *close)(HANDLE);
    BOOL (WINAPI *remove)(LPCWSTR);
    BOOL (WINAPI *flush)(HANDLE);
    BOOL (WINAPI *replace)(LPCWSTR,LPCWSTR,DWORD);
    DWORD (WINAPI *error)(void);
} LidBridgeIoOps;
typedef struct LidBridgeIo {
    LidBridgeIoOps ops;
    wchar_t directory[1000],path[1120],temporary[1160],suffix[40];
    int initialized;
} LidBridgeIo;
static int lid_bridge_io_init(LidBridgeIo* io,const LidBridgeIoOps* ops,
                              const wchar_t* directory,unsigned pid,unsigned long long birth){
    static const wchar_t hex[]=L"0123456789ABCDEF";
    size_t n=0,k=0;int shift;
    if(!io||io->initialized||!ops||!ops->open||!ops->read||!ops->write||
       !ops->close||!ops->remove||!ops->flush||!ops->replace||!ops->error||!directory||!pid||!birth)return 0;
    while(n<999&&directory[n])n++;
    if(!n||n>=999)return 0;
    io->ops=*ops;
    for(k=0;k<n;k++)io->directory[k]=directory[k];
    if(io->directory[n-1]!='\\'&&io->directory[n-1]!='/')io->directory[n++]='\\';
    io->directory[n]=0;k=0;io->suffix[k++]='.';
    for(shift=28;shift>=0;shift-=4)io->suffix[k++]=hex[(pid>>shift)&15];
    io->suffix[k++]='.';
    for(shift=60;shift>=0;shift-=4)io->suffix[k++]=hex[(birth>>shift)&15];
    io->suffix[k++]='.';io->suffix[k++]='t';io->suffix[k++]='m';io->suffix[k++]='p';io->suffix[k]=0;
    io->initialized=1;return 1;
}
static int lid_bridge_io_path(LidBridgeIo* io,int id){
    static const wchar_t* names[]={L"character_native_offer.bin",L"character_client_quiesced.bin",L"character_target.bin",L"character_native_saved.bin"};
    size_t n=0,i;
    if(!io||!io->initialized||id<0||id>3)return 0;
    while(io->directory[n]){io->path[n]=io->directory[n];n++;}
    for(i=0;names[id][i];i++)io->path[n++]=names[id][i];io->path[n]=0;
    for(i=0;i<n;i++)io->temporary[i]=io->path[i];
    for(i=0;io->suffix[i];i++)io->temporary[n++]=io->suffix[i];io->temporary[n]=0;
    return 1;
}
static int lid_bridge_io_read(void* context,int id,BYTE* out,size_t capacity,size_t* size){
    LidBridgeIo* io=context;BYTE data[257];DWORD got=0,error;HANDLE handle;BOOL ok,closed;
    if(size)*size=0;
    if(!out||!size||capacity!=256||(id!=LID_HOST_QUIESCED_FILE&&id!=LID_HOST_TARGET_FILE)||
       !lid_bridge_io_path(io,id))return -1;
    handle=io->ops.open(io->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(handle==INVALID_HANDLE_VALUE){error=io->ops.error();return error==2||error==3?0:-1;}
    ok=io->ops.read(handle,data,257,&got,0);closed=io->ops.close(handle);
    if(!ok||!closed||got!=256)return -1;
    lid_wire_copy(out,data,256);*size=256;return 1;
}
static int lid_bridge_io_publish(void* context,int id,const BYTE* data,size_t size){
    LidBridgeIo* io=context;HANDLE handle;DWORD wrote=0;BOOL ok,closed;
    if(!data||size!=256||(id!=LID_HOST_OFFER_FILE&&id!=LID_HOST_SAVED_FILE)||
       !lid_bridge_io_path(io,id))return 0;
    handle=io->ops.open(io->temporary,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(handle==INVALID_HANDLE_VALUE)return 0;
    ok=io->ops.write(handle,data,256,&wrote,0);
    if(ok&&wrote==256)ok=io->ops.flush(handle);else ok=0;
    closed=io->ops.close(handle);
    if(!ok||!closed||!io->ops.replace(io->temporary,io->path,9)){
        io->ops.remove(io->temporary);return 0;
    }
    return 1;
}
#endif
