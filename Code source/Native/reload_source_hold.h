#ifndef LID_RELOAD_SOURCE_HOLD_H
#define LID_RELOAD_SOURCE_HOLD_H
#include "reload_source_proof.h"
#include "reload_load_trace.h"
/* winlite.h first. Read-only stable file lease using real Win32/CNG signatures.
   Caller closes only AFTER native Load returns. No worker loop/save writes.
   Open shares READ only, so concurrent write/delete/rename is rejected. */
typedef struct LidReloadSourceIo {
    HANDLE (WINAPI *open)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
    BOOL (WINAPI *read)(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
    BOOL (WINAPI *close)(HANDLE);
    void*(*allocate)(size_t);void(*release)(void*);
    LONG (WINAPI *algorithm)(void**,const wchar_t*,const wchar_t*,DWORD);
    LONG (WINAPI *hash)(void*,BYTE*,DWORD,BYTE*,DWORD,BYTE*,DWORD);
    LONG (WINAPI *algorithm_close)(void*,DWORD);
} LidReloadSourceIo;
typedef struct LidReloadSourceHold {
    HANDLE handle;LidReloadSourceProof proof;unsigned active,closed,poisoned;
    BYTE* bytes;unsigned bytes_size; /* owned SHA-verified snapshot until close */
} LidReloadSourceHold;
static int lid_reload_source_close(LidReloadSourceHold*s,const LidReloadSourceIo*o){
    if(!s||!o||!o->close||!s->handle||s->closed||(s->bytes&&!o->release))return 0;
    s->closed=1;s->active=0;
    if(s->bytes){
        o->release(s->bytes);s->bytes=0;s->bytes_size=0;
    }
    if(!o->close(s->handle)){LID_R36_SET(close_error,LID_R36_OS_ERROR());s->poisoned=1;return 0;}
    s->handle=0;return 1;
}
static int lid_reload_source_hold(LidReloadSourceHold*s,const LidReloadSourceIo*o,
                                 const wchar_t*path,const LidReloadSourceProof*p){
    BYTE*bytes=0;BYTE digest[32];DWORD got=0,total=0,want;void*algorithm=0;int good=0;
    LID_R36_SET(hold_stage,1);
    if(!s||s->handle||s->closed||s->active||s->poisoned||s->bytes||s->bytes_size||!o||!o->open||!o->read||!o->close||
       !o->allocate||!o->release||!o->algorithm||!o->hash||!o->algorithm_close||!path||!p||
       p->file_size<24||p->file_size>LID_RELOAD_SOURCE_MAX||!p->uid_length||p->uid_length>128||
       !lid_wire_valid(&p->session)||p->session.kind!=LID_WIRE_QUIESCED)return 0;
    LID_R36_SET(hold_stage,2);
    s->handle=o->open(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(s->handle==INVALID_HANDLE_VALUE){LID_R36_SET(os_error,LID_R36_OS_ERROR());s->handle=0;return 0;}
    if(!s->handle)return 0;
    /* Exactly advertised size +1 catches growth/trailing data; no unbounded read. */
    LID_R36_SET(hold_stage,3);
    bytes=o->allocate((size_t)p->file_size+1);if(!bytes)goto done;
    LID_R36_SET(hold_stage,4);
    while(total<=p->file_size){
        want=p->file_size+1-total;if(want>65536)want=65536;
        got=0;if(!LID_R36_IO(o->read(s->handle,bytes+total,want,&got,0))||got>want)goto done;
        if(!got)break;total+=got;
    }
    LID_R36_SET(hold_stage,5);
    if(total!=p->file_size)goto done;
    LID_R36_SET(hold_stage,6);
    if(LID_R36_RESULT(cng_open_status,o->algorithm(&algorithm,L"SHA256",0,0))<0||!algorithm)goto done;
    LID_R36_SET(hold_stage,7);
    if(LID_R36_RESULT(cng_hash_status,o->hash(algorithm,0,0,bytes,total,digest,32))<0)goto done;
    LID_R36_SET(hold_stage,8);
    good=LID_R36_RESULT(hash_match,lid_wire_equal(digest,p->sha256,32));
done:
    if(algorithm&&LID_R36_RESULT(cng_close_status,o->algorithm_close(algorithm,0))<0)good=0;
    if(bytes&&!good)o->release(bytes);
    if(!good){(void)lid_reload_source_close(s,o);return 0;}
    s->bytes=bytes;s->bytes_size=total;s->proof=*p;s->active=1;LID_R36_SET(hold_stage,9);return 1;
}
static int lid_reload_source_approved(const LidReloadSourceHold*s,const LidWireMessage*session,
                                      const unsigned short*uid,unsigned length){
    unsigned i;
    if(!s||!s->active||s->closed||s->poisoned||!s->handle||!session||
       !uid||!length||length!=s->proof.uid_length||!lid_wire_valid(session)||
       session->kind!=LID_WIRE_QUIESCED||!lid_wire_same_writer(&s->proof.session,session))return 0;
    for(i=0;i<length;i++)if(uid[i]!=s->proof.uid[i])return 0;
    return uid[length]==0;
}
#endif
