"""Candidate worker layered on the existing authenticated CommonClient."""
import asyncio,time
from .CheckService import CheckService,validate_catalogue,id_set
from .CheckWire import Context

def ensure_check_task(ctx,slot_data,log):
    """Checks-only failures must not affect the validated connection/items."""
    try:
        ctx._checks_slot_data=slot_data
        if 'checks' not in slot_data:return
        if getattr(ctx,'_checks_task',None) is None:
            ctx._checks_task=asyncio.create_task(watch_checks(ctx,log),name='LET IT DIE check delivery')
    except Exception as exc:log(ctx.request.bridge_dir,'AP CHECKS: task setup paused_'+type(exc).__name__)

async def watch_checks(ctx,log):
    from .CharacterTransport import read_message,OFFER_FILE,GRANT
    from NetUtils import ClientStatus
    service=None;config=None;loaded=None;state=None;recovered=False
    try:
        while not ctx.exit_event.is_set():
            try:
                data=getattr(ctx,'_checks_slot_data',None)
                if ctx.server is None or not ctx.slot or not ctx.prepared_slot_key:
                    current='waiting_server'
                elif not data or 'checks' not in data:
                    current='no_check_catalogue'
                else:
                    checked=id_set(ctx.checked_locations);missing=id_set(ctx.missing_locations)
                    if data is not loaded:
                        config=validate_catalogue(data,checked,missing);loaded=data
                    if checked&missing or checked|missing!=config.ids:raise ValueError('server check set changed')
                    native=ctx.native_character_transport
                    if native is None:current='waiting_native_transport'
                    else:
                        grant=read_message(native.directory/OFFER_FILE)
                        if grant.kind!=GRANT or not native.matches(grant):
                            current='waiting_verified_native_grant'
                        else:
                            context=Context(grant.pid,grant.creation_time,grant.nonce,grant.identity,grant.config,grant.writer_pid,grant.writer)
                            if service is None:
                                service=CheckService(native.directory,context,config)
                            if service.context!=context or service.config.fingerprint!=config.fingerprint:raise ValueError('checks session changed; restart required')
                            service.confirm(checked)
                            if not recovered:service.recover_old();recovered=True
                            service.poll()
                            await service.publish(time.monotonic(),ctx.send_msgs,int(ClientStatus.CLIENT_GOAL))
                            capability=service.material_capability
                            current='active' if capability is None or capability.status=='ready' else 'active_materials_'+capability.status
            except asyncio.CancelledError:raise
            except Exception as exc:current='paused_'+type(exc).__name__
            if current!=state:
                log(ctx.request.bridge_dir,'AP CHECKS: state='+current);state=current
            await asyncio.sleep(.5)
    finally:
        if service is not None:service.close()

async def stop_check_task(ctx):
    task=getattr(ctx,'_checks_task',None)
    if task is not None:
        task.cancel()
        try:await task
        except asyncio.CancelledError:pass
        ctx._checks_task=None
