#ifndef LID_MATERIAL_EARLY_RUNTIME_R123_H
#define LID_MATERIAL_EARLY_RUNTIME_R123_H
#if LID_MATERIAL_CAPABILITY_ENABLED && LID_CHECKS_DELIVERY_ENABLED && LID_CHECKS_DIAGNOSTIC_ENABLED
static uint32_t WINAPI checks_material_preinput(void*self);
static R26Ops g_material_early_io;static R105Ops g_material_early_crypto;
/* Called from the existing owner-only native_events preflight, before its
   bag guard reports ready. Does not change the preflight's legacy result. */
static int checks_material_early_install(void*exe){
 uint32_t owner=g_get_tid?g_get_tid():0;R108Host*h=&g_material_host;char digest[65];
 if(!exe||!owner||!g_progress_runtime_mutex.ops.thread_id||g_progress_runtime_mutex.ops.thread_id()!=owner||!g_ap_mode_active||!g_v074_cfg.valid)return 0;
 if(g_material_early_attempted)return g_material_input_slot.owner==owner&&r105_slot_owned(&g_material_input_slot,&g_material_input_ops);
 g_material_early_attempted=1;
 void*kernel=lid_find_module_ascii("kernel32.dll");void*bcrypt=lid_find_module_ascii("bcrypt.dll");
 if(!bcrypt&&g_win.LoadLibraryW)bcrypt=g_win.LoadLibraryW(L"bcrypt.dll");
 if(!kernel||!bcrypt)goto failed;
 g_material_early_io.open=g_win.CreateFileW;g_material_early_io.read=g_win.ReadFile;g_material_early_io.close=g_win.CloseHandle;g_material_early_io.thread_id=g_get_tid;
 g_material_early_crypto.io=&g_material_early_io;g_material_early_crypto.module_path=g_win.GetModuleFileNameW;
 g_material_early_crypto.file_size=(__typeof__(g_material_early_crypto.file_size))lid_get_export(kernel,"GetFileSizeEx");
#define R123_CNG(field,name) g_material_early_crypto.field=(__typeof__(g_material_early_crypto.field))lid_get_export(bcrypt,name)
 R123_CNG(open_alg,"BCryptOpenAlgorithmProvider");R123_CNG(property,"BCryptGetProperty");R123_CNG(create_hash,"BCryptCreateHash");R123_CNG(hash_data,"BCryptHashData");R123_CNG(finish_hash,"BCryptFinishHash");R123_CNG(destroy_hash,"BCryptDestroyHash");R123_CNG(close_alg,"BCryptCloseAlgorithmProvider");
#undef R123_CNG
 if(!r105_init(&g_material_owner_measurement,(uint64_t)(ULONG_PTR)exe,owner)||!r119_owner_measure(&g_material_owner_measurement,&g_material_early_crypto,owner,digest)||!r91_module_bind(&g_checks_ops,(uint64_t)(ULONG_PTR)exe,digest,&h->binding))goto failed;
 g_material_activation_hash=&g_material_owner_measurement;
 g_material_input_ops=(R105SlotOps){g_checks_ops,0,checks_chest_thread,checks_material_slot_protect,checks_material_slot_cas};
 if(!r122_input_install(&g_material_input_slot,&g_material_input_ops,&h->binding,g_material_activation_hash,owner,(uint64_t)(ULONG_PTR)checks_material_preinput))goto failed;
 log_ascii("AP MATERIALS R123: pre-input guard installed before bag readiness; dormant until material REQUEST.");return 1;
failed:
 log_ascii("AP MATERIALS R123: pre-input installation FAILED; material activation unavailable; no READY proof.");return 0;
}
typedef struct R123InputContext {uint64_t manager;R81Layout layout;R104Snapshot snapshot;unsigned diagnostic;} R123InputContext;
static int checks_material_input_request_core(void*opaque){
 R123InputContext*c=opaque;R99Capability request;
 c->diagnostic=1;if(!r100_owner_request(&g_checks_delivery.material_mailbox,g_material_input_slot.owner,&request))return 0;
 c->diagnostic=2;if(!checks_material_owner_snapshot(0,c->manager,&c->snapshot))return -1;
 c->diagnostic=3;if(!r15_same_context(&request.context,&c->snapshot.stamp.context))return -1;
 c->layout=g_material_host.layout;
 if(!r81_layout_valid(&c->layout)||c->layout.object_class!=c->snapshot.view.graph.manager.cls){
  c->diagnostic=4;int acquired=checks_chest_scope_begin(0);int ok=r81_bind(&g_checks_ops,c->manager,(uint64_t)(ULONG_PTR)g_checks_diag.exe+0x20f8d60,&c->layout);checks_chest_scope_end(0,acquired);if(!ok)return -1;
 }
 /* Reconciliation requires owner callbacks even before the first material.
    Bind their table before that check; this never resets a receipt or hooks. */
 c->diagnostic=5;if(!checks_material_callback_init(g_material_input_slot.owner)||
  r124_reconcile(&g_material_world,&g_material_host,&c->snapshot,&c->layout)<0)return -1;
 c->diagnostic=6;
 return 1;
}
static int checks_material_input_request(void*opaque){
 LID_PERF_BEGIN(pickup_request_time);int result=checks_material_input_request_core(opaque);
 LID_PERF_END(pickup_request_time,39,"pickup_material_request",result);return result;
}
static int checks_material_input_prepare_core(void*opaque,const R81Sample*sample){
 R123InputContext*c=opaque;R108Host*h=&g_material_host;uint64_t image=(uint64_t)(ULONG_PTR)g_checks_diag.exe;int out=-99;
 c->diagnostic=7;if(sample->manager!=c->manager||!g_material_activation_hash||!r122_owned_module(g_material_activation_hash,&g_material_input_slot,h->slot.installed?&h->slot:0,&g_material_input_ops,&h->binding))return 0;
 /* Canonical internal admission token: actual outer caller already verified
    as pre-input10CCED2. R40 repeats this admission with the real free slot. */
 c->diagnostic=8;if(!checks_material_bag_request((void*)(ULONG_PTR)c->snapshot.view.graph.player.object,-1,image+0x10cd01e,0,&out))return 0;
 c->diagnostic=9;
 return out==0&&h->pickup.pending&&h->manager==c->manager&&checks_material_callback_module(h);
}
static int checks_material_input_prepare(void*opaque,const R81Sample*sample){
 LID_PERF_BEGIN(pickup_prepare_time);int result=checks_material_input_prepare_core(opaque,sample);
 LID_PERF_END(pickup_prepare_time,40,"pickup_material_prepare",result);return result;
}
static uint32_t WINAPI checks_material_preinput(void*self){
 uint64_t caller=(uint64_t)(ULONG_PTR)__builtin_return_address(0),image=g_material_owner_measurement.image;uint32_t tid=g_get_tid?g_get_tid():0;
 uint32_t(*original)(void*)=(uint32_t(*)(void*))(ULONG_PTR)__atomic_load_n(&g_material_input_slot.original,__ATOMIC_ACQUIRE);
 if(!original)return 0;
 if(tid!=g_material_input_slot.owner||caller!=image+0x10cced2)return original(self);
 R123InputContext context={0};context.manager=(uint64_t)(ULONG_PTR)self;
 R121InputOps ops={g_checks_ops,&context,g_material_input_slot.owner,image,context.manager,&context.layout,checks_chest_thread,original,checks_material_input_request,checks_material_input_prepare};
 uint32_t result=r121_preinput(&g_material_input_gate,&ops,context.manager,caller);
 /* Diagnostic only: bounded distinct outcomes, never log ordinary or idle ticks. */
 static unsigned logged,last_key;
 unsigned gate=g_material_input_gate.diagnostic;
 if(gate>=2){
  unsigned key=gate|(context.diagnostic<<4)|(g_material_owner_cache.diagnostic<<8)|(g_material_input_gate.material_stage<<12);
  if(logged<32&&key!=last_key){
   char line[300]={0};size_t pos=0;logged++;last_key=key;
   append_ascii(line,sizeof(line),&pos,"AP MATERIALS R131 INPUT");
   append_ascii(line,sizeof(line),&pos," gate=");append_dec(line,sizeof(line),&pos,gate);
   append_ascii(line,sizeof(line),&pos," request=");append_dec(line,sizeof(line),&pos,context.diagnostic);
   append_ascii(line,sizeof(line),&pos," snapshot=");append_dec(line,sizeof(line),&pos,g_material_owner_cache.diagnostic);
   append_ascii(line,sizeof(line),&pos," bag=");append_dec(line,sizeof(line),&pos,g_material_input_gate.material_stage);
   append_ascii(line,sizeof(line),&pos," allowed=");append_dec(line,sizeof(line),&pos,result!=0);
   log_ascii(line);
  }
 }
 return result;
}
#endif
#endif
