"""Build a LET IT DIE masters.db copy for the Archipelago run.

V0.49 deliberately does not import Python's sqlite3 module.  Archipelago's
embedded Python build may omit that optional stdlib extension.  On Windows we
use the SQLite C API already provided by winsqlite3.dll through ctypes.
"""

from __future__ import annotations

from pathlib import Path
import shutil
import sys


LEVEL_UP_XP_COST = 9_999_999


class _WindowsSQLite:
    """Minimal SQLite wrapper using Windows' winsqlite3.dll."""

    SQLITE_OK = 0
    SQLITE_OPEN_READWRITE = 0x00000002

    def __init__(self, path: Path):
        import ctypes

        self._ctypes = ctypes
        self._lib = ctypes.CDLL("winsqlite3.dll")
        self._db = ctypes.c_void_p()

        self._lib.sqlite3_open_v2.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.c_int,
            ctypes.c_char_p,
        ]
        self._lib.sqlite3_open_v2.restype = ctypes.c_int

        self._lib.sqlite3_exec.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_char_p),
        ]
        self._lib.sqlite3_exec.restype = ctypes.c_int

        self._lib.sqlite3_errmsg.argtypes = [ctypes.c_void_p]
        self._lib.sqlite3_errmsg.restype = ctypes.c_char_p

        self._lib.sqlite3_free.argtypes = [ctypes.c_void_p]
        self._lib.sqlite3_free.restype = None

        self._lib.sqlite3_close.argtypes = [ctypes.c_void_p]
        self._lib.sqlite3_close.restype = ctypes.c_int

        rc = self._lib.sqlite3_open_v2(
            str(path).encode("utf-8"),
            ctypes.byref(self._db),
            self.SQLITE_OPEN_READWRITE,
            None,
        )
        if rc != self.SQLITE_OK:
            message = self._error_message()
            if self._db:
                self._lib.sqlite3_close(self._db)
                self._db = ctypes.c_void_p()
            raise RuntimeError(f"winsqlite3 could not open masters.db ({rc}): {message}")

    def _error_message(self) -> str:
        if not self._db:
            return "unknown SQLite error"
        raw = self._lib.sqlite3_errmsg(self._db)
        return raw.decode("utf-8", errors="replace") if raw else "unknown SQLite error"

    def execute(self, sql: str) -> None:
        ctypes = self._ctypes
        errmsg = ctypes.c_char_p()
        rc = self._lib.sqlite3_exec(
            self._db,
            sql.encode("utf-8"),
            None,
            None,
            ctypes.byref(errmsg),
        )
        if rc != self.SQLITE_OK:
            if errmsg.value:
                message = errmsg.value.decode("utf-8", errors="replace")
            else:
                message = self._error_message()
            if errmsg:
                self._lib.sqlite3_free(ctypes.cast(errmsg, ctypes.c_void_p))
            raise RuntimeError(f"winsqlite3 exec failed ({rc}): {message}")

    def scalar_int(self, sql: str) -> int:
        ctypes = self._ctypes
        value: list[int] = []
        callback_type = ctypes.CFUNCTYPE(
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.POINTER(ctypes.c_char_p),
        )

        @callback_type
        def callback(_user, argc, argv, _columns):
            if argc > 0 and argv and argv[0]:
                value.append(int(argv[0].decode("utf-8", errors="strict")))
            return 0

        errmsg = ctypes.c_char_p()
        rc = self._lib.sqlite3_exec(
            self._db,
            sql.encode("utf-8"),
            ctypes.cast(callback, ctypes.c_void_p),
            None,
            ctypes.byref(errmsg),
        )
        if rc != self.SQLITE_OK:
            if errmsg.value:
                message = errmsg.value.decode("utf-8", errors="replace")
            else:
                message = self._error_message()
            if errmsg:
                self._lib.sqlite3_free(ctypes.cast(errmsg, ctypes.c_void_p))
            raise RuntimeError(f"winsqlite3 query failed ({rc}): {message}")
        if not value:
            raise RuntimeError("winsqlite3 query returned no rows")
        return value[0]

    def close(self) -> None:
        if self._db:
            rc = self._lib.sqlite3_close(self._db)
            self._db = self._ctypes.c_void_p()
            if rc != self.SQLITE_OK:
                raise RuntimeError(f"winsqlite3 close failed ({rc})")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False


def _patch_with_windows_sqlite(destination_db: Path, *, max_index: int,
        starting_hp: int, starting_stm: int, starting_str: int,
        starting_dex: int, starting_vit: int, starting_luk: int,
        hp_increase: int, stm_increase: int, str_increase: int,
        dex_increase: int, vit_increase: int, luk_increase: int,
        skill_slots_increase: int, death_bag_increase: int,
        rage_gauge_increase: int,
        starting_skill_slots: int, starting_death_bag: int,
        starting_rage_gauge: int) -> None:
    """Patch the copied DB through winsqlite3.dll without Python sqlite3."""

    with _WindowsSQLite(destination_db) as db:
        available = db.scalar_int(
            "SELECT COUNT(*) FROM master_bodylvl_status_value "
            f"WHERE type='BAL' AND grade=6 AND lvl BETWEEN 1 AND {int(max_index)};"
        )
        if available != max_index:
            raise RuntimeError(
                f"masters.db does not contain BAL Grade 6 levels 1-{max_index} "
                f"(found {available})."
            )

        skill_value = "" if int(starting_skill_slots) == 0 else ",".join(
            str(index) for index in range(1, int(starting_skill_slots) + 1)
        )

        statements = [
            "BEGIN IMMEDIATE;",
            (
                "UPDATE master_body_detail "
                f"SET param_lv_max={int(max_index)}, "
                f"bag_capacity={int(starting_death_bag)}, "
                f"skill_slots='{skill_value}', "
                f"rage_capacity={int(starting_rage_gauge)} "
                "WHERE type='BAL' AND grade=6 AND limit_break=0;"
            ),
        ]

        for level in range(1, max_index + 1):
            index = level - 1
            statements.append(
                "UPDATE master_bodylvl_status_value SET "
                f"hp={int(starting_hp) + index * int(hp_increase)}, "
                f"stm={int(starting_stm) + index * int(stm_increase)}, "
                f"str={int(starting_str) + index * int(str_increase)}, "
                f"dex={int(starting_dex) + index * int(dex_increase)}, "
                f"vit={int(starting_vit) + index * int(vit_increase)}, "
                f"luk={int(starting_luk) + index * int(luk_increase)}, "
                f"skill={int(starting_skill_slots) + level * int(skill_slots_increase)}, "
                f"bag={int(starting_death_bag) + level * int(death_bag_increase)}, "
                f"rage={int(starting_rage_gauge) + level * int(rage_gauge_increase)} "
                f"WHERE type='BAL' AND grade=6 AND lvl={level};"
            )

        statements.append("COMMIT;")
        try:
            db.execute("\n".join(statements))
        except Exception:
            try:
                db.execute("ROLLBACK;")
            except Exception:
                pass
            raise


def _patch_with_python_sqlite_for_tests(destination_db: Path, **kwargs) -> None:
    """Non-Windows fallback used only for development/tests."""
    import sqlite3

    max_index = int(kwargs["max_index"])
    with sqlite3.connect(destination_db) as db:
        available = db.execute(
            "SELECT COUNT(*) FROM master_bodylvl_status_value "
            "WHERE type='BAL' AND grade=6 AND lvl BETWEEN 1 AND ?",
            (max_index,),
        ).fetchone()[0]
        if int(available) != max_index:
            raise RuntimeError(
                f"masters.db does not contain BAL Grade 6 levels 1-{max_index} "
                f"(found {available})."
            )

        skill_value = "" if int(kwargs["starting_skill_slots"]) == 0 else ",".join(
            str(index) for index in range(1, int(kwargs["starting_skill_slots"]) + 1)
        )
        db.execute(
            "UPDATE master_body_detail SET param_lv_max=?, bag_capacity=?, skill_slots=?, rage_capacity=? "
            "WHERE type='BAL' AND grade=6 AND limit_break=0",
            (max_index, int(kwargs["starting_death_bag"]), skill_value, int(kwargs["starting_rage_gauge"])),
        )
        for level in range(1, max_index + 1):
            index = level - 1
            db.execute(
                "UPDATE master_bodylvl_status_value SET hp=?, stm=?, str=?, dex=?, vit=?, luk=?, skill=?, bag=?, rage=? "
                "WHERE type='BAL' AND grade=6 AND lvl=?",
                (
                    int(kwargs["starting_hp"]) + index * int(kwargs["hp_increase"]),
                    int(kwargs["starting_stm"]) + index * int(kwargs["stm_increase"]),
                    int(kwargs["starting_str"]) + index * int(kwargs["str_increase"]),
                    int(kwargs["starting_dex"]) + index * int(kwargs["dex_increase"]),
                    int(kwargs["starting_vit"]) + index * int(kwargs["vit_increase"]),
                    int(kwargs["starting_luk"]) + index * int(kwargs["luk_increase"]),
                    int(kwargs["starting_skill_slots"]) + level * int(kwargs["skill_slots_increase"]),
                    int(kwargs["starting_death_bag"]) + level * int(kwargs["death_bag_increase"]),
                    int(kwargs["starting_rage_gauge"]) + level * int(kwargs["rage_gauge_increase"]),
                    level,
                ),
            )
        db.commit()


def _set_level_up_xp_cost(db) -> None:
    """Change only XP prices; leave zombie XP, Bloodnium and stats intact."""
    def scalar(sql):
        if isinstance(db, _WindowsSQLite):
            return db.scalar_int(sql)
        return int(db.execute(sql).fetchone()[0])

    if scalar("SELECT COUNT(*) FROM master_bodylvl_exp") == 0:
        raise RuntimeError("masters.db has no fighter level XP costs")
    mismatch = f"SELECT COUNT(*) FROM master_bodylvl_exp WHERE exp <> {LEVEL_UP_XP_COST}"
    if scalar(mismatch) == 0:
        return
    db.execute("BEGIN IMMEDIATE;")
    try:
        db.execute(f"UPDATE master_bodylvl_exp SET exp={LEVEL_UP_XP_COST};")
        if scalar(mismatch) != 0:
            raise RuntimeError("Fighter level XP cost verification failed")
        db.execute("COMMIT;")
    except Exception:
        db.execute("ROLLBACK;")
        raise


def apply_level_up_xp_cost(destination_db: str | Path) -> None:
    """Apply the AP-only price policy to an existing dedicated slot DB.

    Called before the slot's masters copy is used, never on the vanilla DB.
    Already-current copies are read without issuing an UPDATE.
    """
    destination_db = Path(destination_db)
    if not destination_db.is_file():
        raise FileNotFoundError(destination_db)
    if sys.platform == "win32":
        with _WindowsSQLite(destination_db) as db:
            _set_level_up_xp_cost(db)
    else:
        import sqlite3
        from contextlib import closing
        with closing(sqlite3.connect(destination_db)) as db:
            _set_level_up_xp_cost(db)


# Creates the database used while an Archipelago slot is active.
def create_archipelago_masters(
    source_db: str | Path,
    destination_db: str | Path,
    *,
    starting_hp: int = 200,
    starting_stm: int = 200,
    starting_str: int = 10,
    starting_dex: int = 10,
    starting_vit: int = 10,
    starting_luk: int = 10,
    hp_increase: int = 200,
    stm_increase: int = 5,
    str_increase: int = 6,
    dex_increase: int = 6,
    vit_increase: int = 3,
    luk_increase: int = 2,
    skill_slots_increase: int = 1,
    death_bag_increase: int = 1,
    rage_gauge_increase: int = 1,
    stat_progressive_count: int = 30,
    starting_skill_slots: int = 0,
    starting_death_bag: int = 1,
    starting_rage_gauge: int = 0,
) -> Path:
    source_db = Path(source_db)
    destination_db = Path(destination_db)

    if source_db.resolve() == destination_db.resolve():
        raise ValueError("Refusing to patch masters.db in place.")

    shutil.copy2(source_db, destination_db)
    max_index = 1 + int(stat_progressive_count)
    if max_index < 1 or max_index > 45:
        raise RuntimeError(
            "LET IT DIE BAL Grade 6 has native stat rows through level 45; "
            f"requested {max_index} levels ({int(stat_progressive_count)} progressive increases). "
            "Use at most 44 progressive increases for each progression track."
        )

    kwargs = dict(
        max_index=max_index,
        starting_hp=starting_hp, starting_stm=starting_stm,
        starting_str=starting_str, starting_dex=starting_dex,
        starting_vit=starting_vit, starting_luk=starting_luk,
        hp_increase=hp_increase, stm_increase=stm_increase,
        str_increase=str_increase, dex_increase=dex_increase,
        vit_increase=vit_increase, luk_increase=luk_increase,
        skill_slots_increase=skill_slots_increase,
        death_bag_increase=death_bag_increase,
        rage_gauge_increase=rage_gauge_increase,
        starting_skill_slots=starting_skill_slots,
        starting_death_bag=starting_death_bag,
        starting_rage_gauge=starting_rage_gauge,
    )

    if sys.platform == "win32":
        _patch_with_windows_sqlite(destination_db, **kwargs)
    else:
        _patch_with_python_sqlite_for_tests(destination_db, **kwargs)

    apply_level_up_xp_cost(destination_db)
    return destination_db
