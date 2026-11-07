#ifndef LID_DECAL_FILES_H
#define LID_DECAL_FILES_H
/* Candidate fixed bridge files, no game-save path/API. Include bridge I/O and
   decal_ledger first. outside() must prove owner thread AND no engine window. */
enum {LID_DECAL_PENDING_FILE=0,LID_DECAL_LEDGER_FILE=1,LID_DECAL_SAVED_FILE=2,LID_DECAL_TARGET_FILE=3};
typedef struct LidDecalFiles {
    LidBridgeIo*bridge;void*context;int(*outside)(void*);
    BYTE scratch[1153];
} LidDecalFiles;
static int lid_decal_file_path(LidDecalFiles*f,unsigned id){
    static const wchar_t*names[]={L"decal_pending.bin",L"decal_ledger.bin",L"decal_native_saved.bin",L"decal_target.bin"};
    LidBridgeIo*io;size_t n=0,i;
    if(!f||!(io=f->bridge)||!io->initialized||!f->outside||f->outside(f->context)!=1||id>3)return 0;
    while(n<1000&&io->directory[n]){io->path[n]=io->directory[n];n++;}
    if(!n||n>=1000)return 0;
    for(i=0;names[id][i];i++)io->path[n++]=names[id][i];io->path[n]=0;
    for(i=0;i<n;i++)io->temporary[i]=io->path[i];
    for(i=0;i<40&&io->suffix[i];i++)io->temporary[n++]=io->suffix[i];
    if(!i||i==40)return 0;io->temporary[n]=0;return 1;
}
/* 0 absent,1 exact bounded frame,-1 error. Output unchanged unless success. */
static int lid_decal_file_read(LidDecalFiles*f,unsigned id,BYTE*out){
    LidBridgeIo*io;HANDLE h;DWORD n=0,e;BOOL ok,closed;
    if(!out||!lid_decal_file_path(f,id))return -1;io=f->bridge;
    h=io->ops.open(io->path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|4,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE){e=io->ops.error();return e==2||e==3?0:-1;}
    ok=io->ops.read(h,f->scratch,sizeof(f->scratch),&n,0);closed=io->ops.close(h);
    if(!ok||!closed||n!=1152)return -1;
    lid_wire_copy(out,f->scratch,1152);return 1;
}
/* Caller must validate the frame and native authority BEFORE publication.
   The file helper never grants authority. No output write on failed flush. */
static int lid_decal_file_write(LidDecalFiles*f,unsigned id,const BYTE*data){
    LidBridgeIo*io;HANDLE h;DWORD n=0;BOOL ok,closed;
    if(!data||id==LID_DECAL_TARGET_FILE||!lid_decal_file_path(f,id))return 0;io=f->bridge;
    h=io->ops.open(io->temporary,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE)return 0;
    ok=io->ops.write(h,data,1152,&n,0);if(ok&&n==1152)ok=io->ops.flush(h);else ok=0;
    closed=io->ops.close(h);
    if(!ok||!closed||!io->ops.replace(io->temporary,io->path,9)){io->ops.remove(io->temporary);return 0;}
    return 1;
}
/* Only an already-proved pending intent may be removed; callers own that
   policy. This deliberately cannot delete ledger/proof/target or any save. */
static int lid_decal_pending_remove(LidDecalFiles*f){
    DWORD e;if(!lid_decal_file_path(f,LID_DECAL_PENDING_FILE))return 0;
    if(f->bridge->ops.remove(f->bridge->path))return 1;
    e=f->bridge->ops.error();return e==2||e==3;
}
#endif
