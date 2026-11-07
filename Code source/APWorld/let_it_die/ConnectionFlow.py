"""Connection/session identity shared by the menu hook and AP client."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path


@dataclass(frozen=True)
class ArchipelagoConnection:
    """Fields entered by the player in Connect to Archipelago."""

    server: str
    player_name: str
    password: str = ""


@dataclass(frozen=True)
class ArchipelagoSlotIdentity:
    """Stable identity received after Archipelago accepts the connection."""

    server: str
    player_name: str
    seed_name: str
    slot: int


# Creates the deterministic local directory key for one AP slot.
def local_slot_key(identity: ArchipelagoSlotIdentity) -> str:
    payload = (
        f"{identity.server}\n"
        f"{identity.seed_name}\n"
        f"{identity.slot}\n"
        f"{identity.player_name}"
    ).encode("utf-8")

    return hashlib.sha256(payload).hexdigest()[:20]


# Saves non-secret connection identity; passwords are never persisted.
def write_slot_metadata(
    identity: ArchipelagoSlotIdentity,
    destination: str | Path,
) -> Path:
    destination = Path(destination)

    destination.write_text(
        json.dumps(
            {
                "server": identity.server,
                "player_name": identity.player_name,
                "seed_name": identity.seed_name,
                "slot": identity.slot,
                "local_slot_key": local_slot_key(identity),
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    return destination
