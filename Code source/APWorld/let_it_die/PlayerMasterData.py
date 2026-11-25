"""Restore shared native fighter tables in a dedicated AP master copy.

The client checks the paired native capability before enabling this migration.
Never accepts a game save or patches the shipped database.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

from .MasterCompatibility import _identifier, _literal, _open, _rows

_TABLES = (
    ('master_bodylvl_status_value', ('lvl', 'type', 'grade'),
     ('hp', 'str', 'dex', 'vit', 'stm', 'luk', 'skill', 'bag', 'rage'), 45),
    ('master_body_detail', ('type', 'grade', 'limit_break'),
     ('param_lv_max', 'bag_capacity', 'skill_slots', 'rage_capacity'), 5),
)


def _hash(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def _sql_value(value: str | None) -> str:
    return 'NULL' if value is None else _literal(value)


def _read_fighter_rows(db, table: str, keys: tuple[str, ...], expected: int):
    schema = _rows(db, 'PRAGMA table_info(' + _identifier(table) + ')')
    columns = tuple(row[1] for row in schema)
    if len(set(columns)) != len(columns) or not set(keys) <= set(columns):
        raise ValueError('Unknown fighter table schema: ' + table)
    selection = ','.join(map(_identifier, columns))
    records = _rows(db, 'SELECT ' + selection + ' FROM ' + _identifier(table)
        + " WHERE type='BAL' AND grade=6 ORDER BY " + ','.join(map(_identifier, keys)))
    if len(records) != expected:
        raise ValueError('Unexpected BAL Grade 6 row count: ' + table)
    indices = tuple(columns.index(key) for key in keys)
    indexed = {tuple(row[i] for i in indices): row for row in records}
    if len(indexed) != expected:
        raise ValueError('Ambiguous fighter table keys: ' + table)
    index_key = 'lvl' if table == 'master_bodylvl_status_value' else 'limit_break'
    observed = {int(row[columns.index(index_key)]) for row in records}
    wanted = set(range(1, 46)) if index_key == 'lvl' else set(range(5))
    if observed != wanted:
        raise ValueError('Unexpected fighter progression indices: ' + table)
    return schema, columns, indexed


def restore_shared_fighter_tables(destination_db: str | Path, *,
        slot_directory: str | Path, source_db: str | Path) -> dict[str, int | str]:
    """Restore only known AP fighter fields; preserve all unrelated content.

    This is an initialization migration on a dedicated master copy, with no save,
    receipt, ledger or game-process access. The caller must arrange the matching
    native player-only implementation before enabling it in slot preparation.
    """
    destination = Path(destination_db).resolve(strict=True)
    directory = Path(slot_directory).resolve(strict=True)
    source = Path(source_db).resolve(strict=True)
    if (destination.name != 'masters.db' or destination.parent != directory
            or directory.parent.name != 'slots'):
        raise ValueError('Dedicated AP slot master required')
    if destination == source or destination.samefile(source):
        raise ValueError('Original masters database is immutable')
    if not destination.is_file() or not source.is_file():
        raise ValueError('Master database files required')
    source_hash = _hash(source)
    references = []
    with _open(source, True) as original:
        for table, keys, mutable, count in _TABLES:
            schema, columns, records = _read_fighter_rows(original, table, keys, count)
            if not set(mutable) <= set(columns):
                raise ValueError('Missing native fighter fields: ' + table)
            references.append((table, keys, mutable, count, schema, columns, records))
    if _hash(source) != source_hash:
        raise RuntimeError('Native master changed while being read')
    changed = 0
    with _open(destination) as target:
        target.execute('BEGIN IMMEDIATE;')
        try:
            for table, keys, mutable, count, schema, columns, records in references:
                found_schema, found_columns, found = _read_fighter_rows(target, table, keys, count)
                if found_schema != schema or found_columns != columns or found.keys() != records.keys():
                    raise ValueError('Dedicated fighter schema or keys differ: ' + table)
                editable = {columns.index(field) for field in mutable}
                for key, native in records.items():
                    current = found[key]
                    if any(current[i] != native[i] for i in range(len(columns)) if i not in editable):
                        raise ValueError('Unknown fighter data modification: ' + table)
                    if current == native:
                        continue
                    assignments = ','.join(_identifier(field) + '='
                        + _sql_value(native[columns.index(field)]) for field in mutable)
                    where = ' AND '.join(_identifier(field) + '=' + _sql_value(value)
                        for field, value in zip(keys, key))
                    target.execute('UPDATE ' + _identifier(table) + ' SET ' + assignments + ' WHERE ' + where)
                    changed += 1
                _, _, verified = _read_fighter_rows(target, table, keys, count)
                if verified != records:
                    raise RuntimeError('Native fighter restoration readback mismatch: ' + table)
            if _hash(source) != source_hash:
                raise RuntimeError('Native master changed during restoration')
            target.execute('COMMIT;')
        except BaseException:
            target.execute('ROLLBACK;')
            raise
    return {'updated_rows': changed, 'verified_rows': 50, 'source_sha256': source_hash}
def require_player_stats_native(game_root: str | Path) -> str:
    """Check the paired native capability before changing any slot file.

    Data inspection only: never load or execute the DLL. This is a compatibility
    check, not an authenticity/signature verification mechanism.
    """
    native = Path(game_root) / 'Binaries' / 'Win64' / 'Archipelago' / 'LetItDieArchipelago.dll'
    if not native.is_file() or not 1024 <= native.stat().st_size <= 16 * 1024 * 1024:
        raise RuntimeError('The matching player-only statistics DLL must be installed before preparing this AP slot.')
    data = native.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0] if data[:2] == b'MZ' else len(data)
    if (pe > len(data) - 24 or data[pe:pe + 4] != b'PE\0\0'
            or struct.unpack_from('<H', data, pe + 4)[0] != 0x8664
            or not struct.unpack_from('<H', data, pe + 22)[0] & 0x2000
            or b'LID_PLAYER_ONLY_STATS_v1\0' not in data):
        raise RuntimeError('This APWorld requires a compatible player-only statistics DLL; no slot data was migrated.')
    return _hash(native)


