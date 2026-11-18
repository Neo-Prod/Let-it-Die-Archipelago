"""Candidate worker layered on the existing authenticated CommonClient."""
import asyncio,time
from .CheckService import CheckService,validate_catalogue,id_set
from .CheckWire import Context
from .ShopPurchase import ShopPurchaseService
from .ShopRuntimeCatalogue import publish as publish_shop_catalogue
from .ShopRuntimePolicy import publish as publish_shop_policy
from .ShopDisplay import ShopDisplayService

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
    service=None;shops=None;tower_shops=None;display=None;config=None;loaded=None;state=None;recovered=False;catalogue_published=False;policy_published=False
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
                    if checked&missing or checked|missing!=config.all_ids:raise ValueError('server check set changed')
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
                            try:
                                service.confirm(checked&config.ids)
                                if not recovered:service.recover_old();recovered=True
                                service.poll()
                                await service.publish(time.monotonic(),ctx.send_msgs,int(ClientStatus.CLIENT_GOAL))
                                capability=service.material_capability
                                current='active' if capability is None or capability.status=='ready' else 'active_materials_'+capability.status
                            except asyncio.CancelledError:raise
                            except Exception as exc:current='paused_'+type(exc).__name__
                            # Shop errors do not stop death/material delivery or
                            # alter character receipt history / entry dispatch.
                            if not policy_published:
                                try:
                                    publish_shop_policy(native.directory,context,config)
                                    policy_published=True
                                except asyncio.CancelledError:raise
                                except Exception as exc:current+=';shop_policy_paused_'+type(exc).__name__
                            if config.shop_offers:
                                try:
                                    if config.tower_offers and not catalogue_published:
                                        try:
                                            publish_shop_catalogue(native.directory,context,config)
                                            catalogue_published=True
                                        except asyncio.CancelledError:raise
                                        except Exception as exc:current+=';shop_catalogue_paused_'+type(exc).__name__
                                    try:
                                        if display is None:display=ShopDisplayService(native.directory,context,config)
                                        if display.context!=context:raise ValueError('shop display session changed; restart required')
                                        display_state=await display.step(ctx,time.monotonic())
                                        if display_state!='ready':current+=';shop_display_'+display_state
                                    except asyncio.CancelledError:raise
                                    except Exception as exc:current+=';shop_display_paused_'+type(exc).__name__
                                    # Retained paid receipts still drain if the
                                    # catalogue file is unavailable. Native
                                    # activation separately requires its verified
                                    # immutable catalogue, never state alone.
                                    if shops is None:shops=ShopPurchaseService(native.directory,context,config.shop_offers)
                                    if shops.context!=context:raise ValueError('shop session changed; restart required')
                                    await shops.step(checked&config.momoka_ids,time.monotonic(),ctx.send_msgs)
                                except asyncio.CancelledError:raise
                                except Exception as exc:current+=';shop_paused_'+type(exc).__name__
                            if config.tower_offers:
                                try:
                                    if tower_shops is None:
                                        from .ShopTowerPurchase import ShopTowerPurchaseService
                                        tower_shops=ShopTowerPurchaseService(native.directory,context,config.tower_offers,config.goal)
                                    if tower_shops.context!=context:raise ValueError('tower shop session changed; restart required')
                                    await tower_shops.step(checked&config.tower_shop_ids,time.monotonic(),ctx.send_msgs)
                                except asyncio.CancelledError:raise
                                except Exception as exc:current+=';tower_shop_paused_'+type(exc).__name__
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
