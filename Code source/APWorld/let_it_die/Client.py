"""Archipelago client bridge for LET IT DIE Offline Edition.

This client intentionally delegates the network protocol to Archipelago's
CommonClient implementation. Its first milestone is deliberately narrow:
validate a real AP connection, obtain slot_data, create/reuse a dedicated local
slot save and a dedicated patched masters.db, and prepare a one-launch Steam argument for the native file overlay.
"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
import json
import logging
import hashlib
import secrets
from pathlib import Path
import sys
from typing import Any

from CommonClient import CommonContext, server_loop

from .ConnectionFlow import ArchipelagoSlotIdentity, local_slot_key, write_slot_metadata
from .MasterPatch import create_archipelago_masters, apply_level_up_xp_cost
from .StarterSave import StarterSaveSettings, generate_archipelago_save
from .RuntimeDeathBag import watch_and_rebuild_death_bag
from .CharacterProgression import CharacterConfig, IncompleteHistory, ReceiptJournal, receipt_identity
from .CharacterTransport import GAME_SHA, NativeTransport
from .RuntimeControl import RebuildControl, query_parent_creation_time


GAME_NAME = "LET IT DIE"
RESULT_FILENAME = "connection_result.json"


def bridge_log(bridge_dir: Path, message: str) -> None:
    """Append a password-free V0.52 bridge breadcrumb."""
    try:
        bridge_dir.mkdir(parents=True, exist_ok=True)
        with (bridge_dir / "client_bootstrap.log").open("a", encoding="utf-8") as log:
            log.write(message + "\n")
    except Exception:
        pass


@dataclass(frozen=True)
class LaunchRequest:
    server: str
    player_name: str
    game_root: Path
    request_path: Path
    parent_pid: int

    @property
    def bridge_dir(self) -> Path:
        return self.request_path.parent

    @property
    def result_path(self) -> Path:
        return self.bridge_dir / RESULT_FILENAME


# Parses the UTF-16 request written by the native DLL. Password is deliberately
# absent from this file and is passed only in process memory/command line.
def read_launch_request(path: str | Path) -> LaunchRequest:
    request_path = Path(path).resolve()
    values: dict[str, str] = {}

    text = request_path.read_text(encoding="utf-16")
    for raw_line in text.splitlines():
        if not raw_line or "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        values[key.strip().lower()] = value.strip()

    server = values.get("server", "")
    player_name = values.get("player", "")
    game_root_raw = values.get("game_root", "")
    parent_pid_raw = values.get("pid", "0")

    if not server:
        raise RuntimeError("Connection request is missing the server address.")
    if not player_name:
        raise RuntimeError("Connection request is missing the player/slot name.")
    if not game_root_raw:
        raise RuntimeError("Connection request is missing the LET IT DIE game root.")

    game_root = Path(game_root_raw).resolve()
    if not game_root.is_dir():
        raise RuntimeError(f"LET IT DIE game root does not exist: {game_root}")

    try:
        parent_pid = int(parent_pid_raw)
    except ValueError as exc:
        raise RuntimeError(f"Invalid parent pid in connection request: {parent_pid_raw!r}") from exc

    return LaunchRequest(
        server=server,
        player_name=player_name,
        game_root=game_root,
        request_path=request_path,
        parent_pid=parent_pid,
    )


# Writes connection status atomically so the in-game DLL never reads a partial file.
def write_result(request: LaunchRequest, status: str, **payload: Any) -> None:
    request.bridge_dir.mkdir(parents=True, exist_ok=True)
    result = {
        "status": status,
        **payload,
    }
    temp = request.result_path.with_suffix(".tmp")
    temp.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    temp.replace(request.result_path)


# Finds the single live vanilla save used as the account/template shell.
def find_vanilla_save(game_root: Path) -> Path:
    savedata_root = game_root / "Savedata"
    candidates = sorted(savedata_root.glob("*/brggame.sav"))

    if not candidates:
        raise RuntimeError(
            f"No brggame.sav was found under {savedata_root}. "
            "Create/finish the validated post-tutorial vanilla save first."
        )

    if len(candidates) != 1:
        raise RuntimeError(
            "Expected exactly one LET IT DIE save profile under Savedata; "
            f"found {len(candidates)}: {', '.join(str(p) for p in candidates)}"
        )

    return candidates[0]


# Converts Connected.slot_data into the exact starter settings used by the save/master layers.
def settings_from_slot_data(slot_data: dict[str, Any]) -> StarterSaveSettings:
    required = (
        "starting_fighter_class",
        "starting_fighter_grade",
        "starting_hp",
        "starting_stm",
        "starting_str",
        "starting_dex",
        "starting_vit",
        "starting_luk",
        "starting_skill_slots",
        "starting_death_bag",
        "starting_rage_gauge",
    )
    missing = [name for name in required if name not in slot_data]
    if missing:
        raise RuntimeError(f"Archipelago slot_data is missing: {', '.join(missing)}")

    fighter_class = str(slot_data["starting_fighter_class"]).lower()
    if fighter_class not in {"all_rounder", "bal"}:
        raise RuntimeError(
            "This client milestone only supports the configured All-Rounder starter; "
            f"server requested {fighter_class!r}."
        )

    return StarterSaveSettings(
        fighter_type="BAL",
        fighter_grade=int(slot_data["starting_fighter_grade"]),
        hp=int(slot_data["starting_hp"]),
        stm=int(slot_data["starting_stm"]),
        strength=int(slot_data["starting_str"]),
        dex=int(slot_data["starting_dex"]),
        vit=int(slot_data["starting_vit"]),
        luck=int(slot_data["starting_luk"]),
        skill_slots=int(slot_data["starting_skill_slots"]),
        death_bag=int(slot_data["starting_death_bag"]),
        rage_gauge=int(slot_data["starting_rage_gauge"]),
    )


# Creates the dedicated save/master pair on first connection; reconnects reuse it.
def prepare_local_slot(
    request: LaunchRequest,
    *,
    seed_name: str,
    slot: int,
    slot_data: dict[str, Any],
) -> tuple[str, Path, Path, bool]:
    identity = ArchipelagoSlotIdentity(
        server=request.server,
        player_name=request.player_name,
        seed_name=seed_name,
        slot=int(slot),
    )
    key = local_slot_key(identity)

    slot_dir = request.bridge_dir / "slots" / key
    slot_dir.mkdir(parents=True, exist_ok=True)

    ap_save = slot_dir / "brggame.sav"
    ap_masters = slot_dir / "masters.db"
    metadata = slot_dir / "slot.json"

    created = False
    if not ap_save.exists() or not ap_masters.exists():
        if ap_save.exists() != ap_masters.exists():
            raise RuntimeError(
                f"Incomplete AP slot {key}: save/master pair is inconsistent. "
                "Delete this slot directory and reconnect."
            )

        settings = settings_from_slot_data(slot_data)
        vanilla_save = find_vanilla_save(request.game_root)
        vanilla_masters = request.game_root / "BrgGame" / "Content" / "masters.db"

        if not vanilla_masters.is_file():
            raise RuntimeError(f"Vanilla masters.db not found: {vanilla_masters}")

        generate_archipelago_save(vanilla_save, ap_save, settings)
        create_archipelago_masters(
            vanilla_masters,
            ap_masters,
            starting_hp=settings.hp,
            starting_stm=settings.stm,
            starting_str=settings.strength,
            starting_dex=settings.dex,
            starting_vit=settings.vit,
            starting_luk=settings.luck,
            starting_skill_slots=settings.skill_slots,
            starting_death_bag=settings.death_bag,
            starting_rage_gauge=settings.rage_gauge,
            hp_increase=int(slot_data.get("progressive_hp_increase", 200)),
            stm_increase=int(slot_data.get("progressive_stm_increase", 5)),
            str_increase=int(slot_data.get("progressive_str_increase", 6)),
            dex_increase=int(slot_data.get("progressive_dex_increase", 6)),
            vit_increase=int(slot_data.get("progressive_vit_increase", 3)),
            luk_increase=int(slot_data.get("progressive_luck_increase", 2)),
            skill_slots_increase=int(slot_data.get("progressive_skill_slots_increase", 1)),
            death_bag_increase=int(slot_data.get("progressive_death_bag_increase", 1)),
            rage_gauge_increase=int(slot_data.get("progressive_rage_gauge_increase", 1)),
            stat_progressive_count=max(
                int(slot_data.get("progressive_hp_quantity", 0)),
                int(slot_data.get("progressive_stm_quantity", 0)),
                int(slot_data.get("progressive_str_quantity", 0)),
                int(slot_data.get("progressive_dex_quantity", 0)),
                int(slot_data.get("progressive_vit_quantity", 0)),
                int(slot_data.get("progressive_luck_quantity", 0)),
                int(slot_data.get("progressive_death_bag_quantity", 0)),
                int(slot_data.get("progressive_skill_slots_quantity", 0)),
                int(slot_data.get("progressive_rage_gauge_quantity", 0)),
            ),
        )
        created = True

    if not created:
        # Update only this slot's dedicated master copy; retain its save.
        apply_level_up_xp_cost(ap_masters)

    write_slot_metadata(identity, metadata)

    # V0.43 activation is passed as a one-launch Steam argument. No marker file is
    # read by the game during DllMain, so vanilla launches remain untouched.
    return key, ap_save, ap_masters, created


# Waits for the current LET IT DIE process to exit without relaunching it.
def wait_for_parent_exit(request: LaunchRequest) -> None:
    if sys.platform != "win32" or request.parent_pid <= 0:
        return

    import ctypes

    SYNCHRONIZE = 0x00100000
    INFINITE = 0xFFFFFFFF
    kernel32 = ctypes.windll.kernel32
    handle = kernel32.OpenProcess(SYNCHRONIZE, False, request.parent_pid)
    if not handle:
        return
    try:
        kernel32.WaitForSingleObject(handle, INFINITE)
    finally:
        kernel32.CloseHandle(handle)


class LetItDieContext(CommonContext):
    game = GAME_NAME
    items_handling = 0b111
    want_slot_data = True

    def __init__(self, request: LaunchRequest, password: str):
        super().__init__(request.server, password or None)
        self.request = request
        self.username = request.player_name
        self.auth = request.player_name
        self.connected_event = asyncio.Event()
        self.setup_error: str | None = None
        self.prepared_slot_key: str | None = None
        # Archipelago 0.6.7 does not expose CommonContext.server_seed_name.
        # Capture RoomInfo.seed_name ourselves in on_package for stable slot identity.
        self.ap_server_seed_name: str = ""
        self.character_receipts: ReceiptJournal | None = None
        self.character_history_ready = False
        self.character_receipt_error: str | None = None
        self._history_probe_marker: str | None = None
        self._history_probe_attempts = 0
        self._history_probe_time = 0.0
        self.runtime_rebuild_control = RebuildControl()
        self.runtime_death_bag_task: asyncio.Task | None = None
        self.runtime_bridge_task: asyncio.Task | None = None
        self.native_character_transport: NativeTransport | None = None
        self._verified_character_game: bool | None = None
        self._character_bridge_state: str | None = None

    def on_print_json(self, args: dict):
        super().on_print_json(args)
        try:
            from .NotificationClient import capture_notification
            capture_notification(self, args, bridge_log)
        except Exception as exc:
            bridge_log(self.request.bridge_dir, 'AP NOTIFY: capture paused_' + type(exc).__name__)

    def _verify_character_game(self) -> bool:
        if self._verified_character_game is None:
            executable = self.request.game_root / "Binaries" / "Win64" / "BrgGame-Steam.exe"
            with executable.open("rb") as stream:
                self._verified_character_game = hashlib.file_digest(stream, "sha256").hexdigest() == GAME_SHA
        return self._verified_character_game

    def _ensure_runtime_item_tasks(self, slot_data: dict) -> None:
        """Singleton tasks across AP network reconnects; no connection error side effects."""
        try:
            asyncio.get_running_loop()
            asyncio.get_running_loop()
            if self.runtime_death_bag_task is None:
                creation = query_parent_creation_time(self.request.parent_pid)
                if creation:
                    self.runtime_rebuild_control.bind_parent(self.request.parent_pid, creation)
                self.runtime_death_bag_task = asyncio.create_task(
                    watch_and_rebuild_death_bag(bridge_dir=self.request.bridge_dir,
                        parent_pid=self.request.parent_pid, ap_capacity=int(slot_data["starting_death_bag"]),
                        control=self.runtime_rebuild_control),
                    name="LET IT DIE Death Bag full runtime rebuild")
            if self.runtime_bridge_task is None:
                self.runtime_bridge_task = asyncio.create_task(self._run_character_bridge(),
                    name="LET IT DIE character item bridge")
        except Exception as exc:
            bridge_log(self.request.bridge_dir, f"AP ITEMS: runtime task setup refused ({type(exc).__name__}); connection unchanged.")
        # Independent check worker; its failures never change connection/items.
        try:
            from .CheckClient import ensure_check_task
            ensure_check_task(self, slot_data, bridge_log)
        except Exception as exc:
            bridge_log(self.request.bridge_dir, 'AP CHECKS: setup paused_' + type(exc).__name__)
        try:
            from .KillCoinClient import ensure_coin_task
            ensure_coin_task(self, bridge_log)
        except Exception as exc:
            bridge_log(self.request.bridge_dir, 'AP KC: setup paused_' + type(exc).__name__)

    async def _run_character_bridge(self) -> None:
        try:
            while not self.exit_event.is_set():
                try:
                    if self.character_history_ready and self.character_receipt_error is not None:
                        self._publish_character_receipts()
                    journal = self.character_receipts
                    key = self.prepared_slot_key
                    if journal is not None and key is not None:
                        await self._request_character_history_barrier()
                        directory = self.request.bridge_dir / "slots" / key
                        if self.native_character_transport is None or self.native_character_transport.directory != directory:
                            self.native_character_transport = NativeTransport(directory, self.request.parent_pid,
                                self.runtime_rebuild_control, self._verify_character_game, blueprints=True,
                                reload_source=True)
                        state = self.native_character_transport.step(journal,
                            self.character_history_ready and self.character_receipt_error is None)
                    else:
                        state = "waiting_valid_receipt_journal"
                except Exception as exc:
                    state = f"bridge_paused_{type(exc).__name__}"
                if state != self._character_bridge_state:
                    bridge_log(self.request.bridge_dir, f"AP ITEMS: bridge state={state}; scope=character and blueprint progressives.")
                    self._character_bridge_state = state
                await asyncio.sleep(0.25)
        finally:
            # Request only. The real thread's finally and remote-call tracker
            # determine quiescence, never cancellation of this asyncio task.
            self.runtime_rebuild_control.request_stop()

    def _prepare_character_receipts(self, slot_data: dict, seed_name: str, key: str) -> None:
        """Receipt-only extension. Failure must never change the connection result."""
        self.character_receipts = None
        self.character_history_ready = False
        self._history_probe_marker = None
        self._history_probe_attempts = 0
        self._history_probe_time = 0.0
        self.character_receipt_error = None
        try:
            config = CharacterConfig.from_slot_data(slot_data)
            identity = receipt_identity(server=self.request.server, seed=seed_name,
                                        team=self.team, slot=self.slot)
            path = self.request.bridge_dir / "slots" / key / "character_receipts.json"
            self.character_receipts = ReceiptJournal(path, identity=identity, config=config)
            bridge_log(self.request.bridge_dir, "AP ITEMS: character receipt journal ready; waiting for index0 history; no gameplay delivery.")
        except Exception as exc:
            self.character_receipt_error = type(exc).__name__
            bridge_log(self.request.bridge_dir, f"AP ITEMS: receipt journal unavailable ({type(exc).__name__}); connection unchanged; no delivery.")

    async def _request_character_history_barrier(self) -> None:
        """Installed server omits empty ReceivedItems, even for Sync.

        Its websocket loop awaits commands in order. Sync then read-only Get
        gives a response barrier; only that response can establish empty history.
        No storage Set, timeout inference, or authentication changes.
        """
        if self.character_receipts is None or self.character_history_ready:
            return
        now = asyncio.get_running_loop().time()
        if self._history_probe_attempts >= 3 or (self._history_probe_attempts and
                now - self._history_probe_time < 5.0):
            return
        marker = "lid_character_history_probe_" + secrets.token_hex(16)
        self._history_probe_marker = marker
        self._history_probe_time = now
        self._history_probe_attempts += 1
        try:
            await self.send_msgs([{"cmd": "Sync"}, {"cmd": "Get", "keys": [marker]}])
        except Exception as exc:
            if self._history_probe_marker == marker:
                self._history_probe_marker = None
            bridge_log(self.request.bridge_dir,
                       f"AP ITEMS: history barrier request deferred ({type(exc).__name__}); no empty-history inference.")

    def _receive_character_history_barrier(self, args: dict) -> None:
        marker = self._history_probe_marker
        keys = args.get("keys")
        if (not marker or self.character_receipts is None or self.character_history_ready or
                not isinstance(keys, dict) or marker not in keys or keys[marker] is not None):
            return
        self._history_probe_marker = None
        if self.items_received:
            # Without an actual index0 packet, a nonempty partial list is not
            # a full history. Never reduce a previous persisted journal.
            return
        self.character_history_ready = True
        self._publish_character_receipts()
        if self.character_receipt_error is None:
            bridge_log(self.request.bridge_dir,
                       "AP ITEMS: empty history confirmed by ordered Sync/Get barrier; receipt-only, no game-save proof.")

    def _receive_character_items(self, args: dict) -> None:
        if self.character_receipts is None:
            return
        index = args.get("index")
        packet_items = args.get("items")
        if type(index) is not int or index < 0 or not isinstance(packet_items, (list, tuple)):
            return
        # CommonClient has already processed this packet. Ignore packets it did
        # not append and require an initial index0 snapshot after every Connected.
        if len(self.items_received) != index + len(packet_items):
            return
        if index == 0:
            self.character_history_ready = True
        if not self.character_history_ready:
            return
        self._publish_character_receipts()

    def _publish_character_receipts(self) -> None:
        if self.character_receipts is None or not self.character_history_ready:
            return
        previous_error = self.character_receipt_error
        try:
            snapshot, changed = self.character_receipts.observe(self.items_received)
            self.character_receipt_error = None
            if changed:
                counts = "/".join(str(n) for n in snapshot.counts)
                bridge_log(self.request.bridge_dir, f"AP ITEMS: received character counts={counts}; pending gameplay delivery; not a game-save acknowledgement.")
        except IncompleteHistory:
            self.character_receipt_error = "IncompleteHistory"
            if previous_error != self.character_receipt_error:
                bridge_log(self.request.bridge_dir, "AP ITEMS: history incomplete; keeping previous receipt journal; no delivery.")
        except Exception as exc:
            # Keep last receipt snapshot and retry publication on the next
            # cumulative update. Never overwrite corruption with starting zeros.
            self.character_receipt_error = type(exc).__name__
            if previous_error != self.character_receipt_error:
                bridge_log(self.request.bridge_dir, f"AP ITEMS: receipt publication refused ({type(exc).__name__}); no delivery.")

    async def _watch_parent_lifetime(self) -> None:
        await asyncio.to_thread(wait_for_parent_exit, self.request)
        bridge_log(self.request.bridge_dir, "V0.52 parent process exited; closing Archipelago client")
        self.exit_event.set()

    async def server_auth(self, password_requested: bool = False):
        bridge_log(self.request.bridge_dir, f"V0.52 server_auth: RoomInfo received; password_requested={bool(password_requested)}")
        if password_requested and not self.password:
            self.setup_error = "The Archipelago server requires a password."
            self.connected_event.set()
            return
        await self.send_connect()

    def event_invalid_slot(self):
        self.setup_error = "Invalid Archipelago slot/player name."
        self.connected_event.set()

    def event_invalid_game(self):
        self.setup_error = "The selected slot is not a LET IT DIE world."
        self.connected_event.set()

    def on_package(self, cmd: str, args: dict):
        if cmd == "ReceivedItems":
            self._receive_character_items(args)
            return
        if cmd == "Retrieved":
            self._receive_character_history_barrier(args)
            return
        # Archipelago 0.6.7 carries the multiworld seed name in RoomInfo but
        # does not persist it on CommonContext. Save it before Connected arrives.
        if cmd == "RoomInfo":
            self.ap_server_seed_name = str(args.get("seed_name") or "")
            bridge_log(
                self.request.bridge_dir,
                f"V0.52 on_package: RoomInfo captured; seed_name={self.ap_server_seed_name!r}",
            )
            return

        if cmd != "Connected":
            return

        bridge_log(self.request.bridge_dir, "V0.52 on_package: Connected received; preparing dedicated AP slot")
        try:
            slot_data = args.get("slot_data") or {}
            # Future AP releases may expose server_seed_name again; prefer our
            # explicit RoomInfo capture but retain compatible fallbacks.
            seed_name = str(
                self.ap_server_seed_name
                or getattr(self, "server_seed_name", "")
                or getattr(self, "seed_name", "")
                or ""
            )
            slot = int(self.slot or 0)

            if not seed_name:
                raise RuntimeError("RoomInfo did not provide a seed_name.")
            if slot <= 0:
                raise RuntimeError("Connected packet did not provide a valid slot number.")

            key, ap_save, ap_masters, created = prepare_local_slot(
                self.request,
                seed_name=seed_name,
                slot=slot,
                slot_data=slot_data,
            )
            self.prepared_slot_key = key
            bridge_log(self.request.bridge_dir, f"V0.52 on_package: AP slot prepared; key={key}; created={created}")

            write_result(
                self.request,
                "ok",
                seed_name=seed_name,
                slot=slot,
                player_name=self.request.player_name,
                slot_key=key,
                created=created,
                save=str(ap_save),
                masters=str(ap_masters),

                # V0.74: publish the exact generated slot configuration to the
                # in-process DLL. No starting stat or progressive increase is
                # compiled into the native mod anymore.
                starting_fighter_grade=int(slot_data["starting_fighter_grade"]),
                starting_hp=int(slot_data["starting_hp"]),
                starting_stm=int(slot_data["starting_stm"]),
                starting_str=int(slot_data["starting_str"]),
                starting_dex=int(slot_data["starting_dex"]),
                starting_vit=int(slot_data["starting_vit"]),
                starting_luk=int(slot_data["starting_luk"]),
                starting_skill_slots=int(slot_data["starting_skill_slots"]),
                starting_death_bag=int(slot_data["starting_death_bag"]),
                starting_rage_gauge=int(slot_data["starting_rage_gauge"]),

                progressive_hp_increase=int(slot_data["progressive_hp_increase"]),
                progressive_stm_increase=int(slot_data["progressive_stm_increase"]),
                progressive_str_increase=int(slot_data["progressive_str_increase"]),
                progressive_dex_increase=int(slot_data["progressive_dex_increase"]),
                progressive_vit_increase=int(slot_data["progressive_vit_increase"]),
                progressive_luck_increase=int(slot_data["progressive_luck_increase"]),
                progressive_death_bag_increase=int(slot_data["progressive_death_bag_increase"]),
                progressive_skill_slots_increase=int(slot_data["progressive_skill_slots_increase"]),
                progressive_rage_gauge_increase=int(slot_data["progressive_rage_gauge_increase"]),

                progressive_hp_quantity=int(slot_data["progressive_hp_quantity"]),
                progressive_stm_quantity=int(slot_data["progressive_stm_quantity"]),
                progressive_str_quantity=int(slot_data["progressive_str_quantity"]),
                progressive_dex_quantity=int(slot_data["progressive_dex_quantity"]),
                progressive_vit_quantity=int(slot_data["progressive_vit_quantity"]),
                progressive_luck_quantity=int(slot_data["progressive_luck_quantity"]),
                progressive_death_bag_quantity=int(slot_data["progressive_death_bag_quantity"]),
                progressive_skill_slots_quantity=int(slot_data["progressive_skill_slots_quantity"]),
                progressive_rage_gauge_quantity=int(slot_data["progressive_rage_gauge_quantity"]),
            )

            # Retain the legacy rebuild until a compatible native peer requests
            # and receives confirmed ownership. No title/auth result is changed.
            self._prepare_character_receipts(slot_data, seed_name, key)
            self._ensure_runtime_item_tasks(slot_data)

            # V0.52 keeps LET IT DIE in the same process. The native proxy activates
            # the prepared slot after consuming this result; this watcher only closes
            # the external AP client when the game process eventually exits.
            asyncio.create_task(
                self._watch_parent_lifetime(),
                name="LET IT DIE parent lifetime watcher",
            )
        except Exception as exc:  # result must always reach the native side
            self.setup_error = str(exc)
            bridge_log(self.request.bridge_dir, f"V0.52 on_package: slot preparation failed: {type(exc).__name__}: {exc}")
            write_result(self.request, "error", message=self.setup_error)
        finally:
            self.connected_event.set()


async def run_client(request_path: str, password: str = "") -> None:
    request_file = Path(request_path).resolve()
    bridge_log(request_file.parent, f"V0.52 run_client: reading request {request_file}")
    request = read_launch_request(request_file)
    bridge_log(request.bridge_dir, f"V0.52 run_client: request parsed; server={request.server}; player={request.player_name}")

    # Remove stale result from a previous attempt before opening the socket.
    try:
        request.result_path.unlink()
    except FileNotFoundError:
        pass

    ctx = LetItDieContext(request, password)
    bridge_log(request.bridge_dir, "V0.52 run_client: CommonContext created")
    ctx.server_task = asyncio.create_task(server_loop(ctx), name="LET IT DIE AP server loop")
    bridge_log(request.bridge_dir, "V0.52 run_client: server_loop task started")

    try:
        await asyncio.wait_for(ctx.connected_event.wait(), timeout=20.0)
    except asyncio.TimeoutError:
        bridge_log(request.bridge_dir, "V0.52 run_client: authentication wait timed out")
        write_result(
            request,
            "error",
            message="Timed out while waiting for Archipelago authentication.",
        )
        await ctx.shutdown()
        return

    if ctx.setup_error:
        if not request.result_path.exists():
            write_result(request, "error", message=ctx.setup_error)
        await ctx.shutdown()
        return

    # Keep the official AP connection alive. Character receipts are journaled,
    # but gameplay application/checks await the native delivery coordinator.
    await ctx.exit_event.wait()
    try:
        from .CheckClient import stop_check_task
        await stop_check_task(ctx)
    except Exception as exc:
        bridge_log(ctx.request.bridge_dir, 'AP CHECKS: stop paused_' + type(exc).__name__)
    try:
        from .KillCoinClient import stop_coin_task
        await stop_coin_task(ctx)
    except Exception as exc:
        bridge_log(ctx.request.bridge_dir, 'AP KC: stop paused_' + type(exc).__name__)
    try:
        from .NotificationClient import stop_notification_task
        await stop_notification_task(ctx)
    except Exception as exc:
        bridge_log(ctx.request.bridge_dir, 'AP NOTIFY: stop paused_' + type(exc).__name__)
    await ctx.shutdown()


# Launcher component entry point. The native mod supplies request path and
# password; password is never written into request/slot metadata.
def launch_from_archipelago(*args: str) -> None:
    if not args:
        raise RuntimeError("LET IT DIE Client requires a connection request path.")

    request_path = args[0]
    password = args[1] if len(args) > 1 else ""
    request_file = Path(request_path).resolve()
    bridge_dir = request_file.parent
    bridge_dir.mkdir(parents=True, exist_ok=True)

    # V0.52 startup breadcrumb. It contains no password and proves that the
    # registered Archipelago component actually reached Client.py.
    bridge_log(bridge_dir, f"V0.52 Client.py entered; request={request_file}")

    try:
        asyncio.run(run_client(str(request_file), password))
    except Exception as exc:
        # Never leave the native bridge waiting for a file if Python aborts
        # before run_client() can publish its normal result.
        message = f"LET IT DIE client failed before connection completed: {type(exc).__name__}: {exc}"
        try:
            temp = bridge_dir / "connection_result.tmp"
            result = bridge_dir / RESULT_FILENAME
            temp.write_text(
                json.dumps({"status": "error", "message": message}, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
            temp.replace(result)
            with (bridge_dir / "client_bootstrap.log").open("a", encoding="utf-8") as log:
                log.write(message + "\n")
        finally:
            password = ""
        return
    finally:
        password = ""


if __name__ == "__main__":
    launch_from_archipelago(*sys.argv[1:])
