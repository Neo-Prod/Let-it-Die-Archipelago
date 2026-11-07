"""Legacy V0.88 Death Bag rebuild with a cooperative native ownership handoff.

The editable R10 path keeps the legacy rebuild until a compatible native peer
requests ownership. A shared process-birth-scoped mutex excludes other writers.
NON_TITLE is a legacy address hint, not proof that gameplay has started; the new
native coordinator must apply the separate R9 readiness checks. Baseline is intact.

Reverse-engineered layout on the validated Steam executable:
    BrgDbChara + 0x18C : TArray.Data (Death Bag entries)
    BrgDbChara + 0x194 : TArray.Num
    BrgDbChara + 0x198 : TArray.Max
    entry size         : 0x44 bytes
    entry + 0x04/14/24/34 : four FString values

The outer TArray is reallocated with LET IT DIE's own allocator.  FString
allocations belonging only to removed slots are captured before resize and
released afterward. Data/Num/Max are committed in the remote resize routine.
"""

from __future__ import annotations

import asyncio
import ctypes
from ctypes import wintypes
from pathlib import Path
import re
import struct
import sys
import time
from .RuntimeControl import RebuildControl, runtime_mutex_name


_NON_TITLE_RE = re.compile(
    r"V0\.70 RAM: phase=NON_TITLE\s+userdata=0x([0-9A-Fa-f]+)\s+"
    r"body=0x([0-9A-Fa-f]+)\s+index=(-?\d+)\b"
)

# Process access used only after authentication and only after NON_TITLE.
_PROCESS_CREATE_THREAD = 0x0002
_PROCESS_QUERY_INFORMATION = 0x0400
_PROCESS_VM_OPERATION = 0x0008
_PROCESS_VM_READ = 0x0010
_PROCESS_VM_WRITE = 0x0020
_SYNCHRONIZE = 0x00100000
_PROCESS_RIGHTS = (
    _PROCESS_CREATE_THREAD
    | _PROCESS_QUERY_INFORMATION
    | _PROCESS_VM_OPERATION
    | _PROCESS_VM_READ
    | _PROCESS_VM_WRITE
    | _SYNCHRONIZE
)

_MEM_COMMIT = 0x1000
_MEM_RESERVE = 0x2000
_MEM_RELEASE = 0x8000
_PAGE_EXECUTE_READWRITE = 0x40
_STILL_ACTIVE = 259
_WAIT_OBJECT_0 = 0x00000000
_WAIT_TIMEOUT = 0x00000102
_INFINITE = 0xFFFFFFFF
_TH32CS_SNAPMODULE = 0x00000008
_TH32CS_SNAPMODULE32 = 0x00000010
_INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

# Exact validated BrgGame-Steam.exe RVAs.
_GAME_IMAGE_BASE = 0x140000000
_GAME_REALLOC_RVA = 0x1400E06F0 - _GAME_IMAGE_BASE
_GAME_FREE_RVA = 0x1400DFDF0 - _GAME_IMAGE_BASE

# Exact Death Bag layout in BrgDbChara.
_DEATH_BAG_ARRAY_OFFSET = 0x18C
_DEATH_BAG_NUM_OFFSET = 0x194
_DEATH_BAG_MAX_OFFSET = 0x198
_DEATH_BAG_ENTRY_SIZE = 0x44
_DEATH_BAG_FSTRING_OFFSETS = (0x04, 0x14, 0x24, 0x34)
_BRGT_DBCHARA_SIZE = 0x220

# Exact UBrgUserData -> persistent Chara path already used by validated V0.82.
_USERDATA_PAYLOAD_OFFSET = 0x74
_PAYLOAD_CHARA_DATA_OFFSET = 0x2EC
_PAYLOAD_CHARA_NUM_OFFSET = 0x2F4

_MAX_REASONABLE_SLOTS = 256
_MAX_REASONABLE_STRING_CAPACITY = 32768

# Function bytes are checked before any allocator call.  If the executable does
# not match the reverse-engineered build, V0.88 refuses to modify memory.
_REALLOC_PREFIX = bytes.fromhex("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20")
_FREE_PREFIX = bytes.fromhex("40 53 48 83 EC 20 48 8B D9")


class _MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.c_void_p),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", wintypes.HMODULE),
        ("szModule", wintypes.WCHAR * 256),
        ("szExePath", wintypes.WCHAR * wintypes.MAX_PATH),
    ]


def _append_bridge_log(bridge_dir: Path, message: str) -> None:
    """Append V0.88 runtime diagnostics without touching native mod.log."""
    try:
        bridge_dir.mkdir(parents=True, exist_ok=True)
        with (bridge_dir / "client_bootstrap.log").open("a", encoding="utf-8") as log:
            log.write(message + "\n")
    except Exception:
        pass


def _configure_kernel32():
    """Configure Win32 APIs required by the external NON_TITLE rebuild."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    kernel32.GetProcessTimes.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.FILETIME),
                                       ctypes.POINTER(wintypes.FILETIME), ctypes.POINTER(wintypes.FILETIME),
                                       ctypes.POINTER(wintypes.FILETIME)]
    kernel32.GetProcessTimes.restype = wintypes.BOOL
    kernel32.CreateMutexW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.LPCWSTR]
    kernel32.CreateMutexW.restype = wintypes.HANDLE
    kernel32.ReleaseMutex.argtypes = [wintypes.HANDLE]
    kernel32.ReleaseMutex.restype = wintypes.BOOL

    kernel32.ReadProcessMemory.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    kernel32.ReadProcessMemory.restype = wintypes.BOOL
    kernel32.WriteProcessMemory.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    kernel32.WriteProcessMemory.restype = wintypes.BOOL

    kernel32.VirtualAllocEx.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD,
    ]
    kernel32.VirtualAllocEx.restype = ctypes.c_void_p
    kernel32.VirtualFreeEx.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, wintypes.DWORD,
    ]
    kernel32.VirtualFreeEx.restype = wintypes.BOOL

    kernel32.CreateRemoteThread.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
        ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
    ]
    kernel32.CreateRemoteThread.restype = wintypes.HANDLE
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL

    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(_MODULEENTRY32W)]
    kernel32.Module32FirstW.restype = wintypes.BOOL
    kernel32.Module32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(_MODULEENTRY32W)]
    kernel32.Module32NextW.restype = wintypes.BOOL
    return kernel32


def _win_error(prefix: str) -> RuntimeError:
    return RuntimeError(f"{prefix} (Win32 error {ctypes.get_last_error()})")


def _read_exact(kernel32, process, address: int, size: int) -> bytes:
    if int(address) <= 0 or int(size) < 0:
        raise RuntimeError(f"invalid remote read 0x{int(address):X} size={size}")
    buffer = (ctypes.c_ubyte * int(size))()
    read = ctypes.c_size_t(0)
    if not kernel32.ReadProcessMemory(
        process, ctypes.c_void_p(int(address)), ctypes.byref(buffer), int(size), ctypes.byref(read)
    ):
        raise _win_error(f"ReadProcessMemory failed at 0x{int(address):X}")
    if int(read.value) != int(size):
        raise RuntimeError(
            f"ReadProcessMemory returned {read.value}/{size} bytes at 0x{int(address):X}"
        )
    return bytes(buffer)


def _write_exact(kernel32, process, address: int, data: bytes) -> None:
    raw = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
    written = ctypes.c_size_t(0)
    if not kernel32.WriteProcessMemory(
        process, ctypes.c_void_p(int(address)), ctypes.byref(raw), len(data), ctypes.byref(written)
    ):
        raise _win_error(f"WriteProcessMemory failed at 0x{int(address):X}")
    if int(written.value) != len(data):
        raise RuntimeError(
            f"WriteProcessMemory returned {written.value}/{len(data)} bytes at 0x{int(address):X}"
        )


def _process_is_alive(kernel32, process) -> bool:
    code = wintypes.DWORD(0)
    return bool(
        kernel32.GetExitCodeProcess(process, ctypes.byref(code))
        and int(code.value) == _STILL_ACTIVE
    )


def _find_game_module_base(kernel32, pid: int) -> int:
    """Resolve BrgGame-Steam.exe ASLR base without using or modifying native hooks."""
    snapshot = kernel32.CreateToolhelp32Snapshot(
        _TH32CS_SNAPMODULE | _TH32CS_SNAPMODULE32, int(pid)
    )
    snapshot_value = ctypes.cast(snapshot, ctypes.c_void_p).value
    if not snapshot_value or int(snapshot_value) == int(_INVALID_HANDLE_VALUE):
        raise _win_error("CreateToolhelp32Snapshot failed")
    try:
        entry = _MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(_MODULEENTRY32W)
        ok = kernel32.Module32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            if str(entry.szModule).lower() == "brggame-steam.exe":
                base = int(entry.modBaseAddr or 0)
                if base <= 0:
                    raise RuntimeError("BrgGame-Steam.exe module has a null base")
                return base
            ok = kernel32.Module32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    raise RuntimeError("BrgGame-Steam.exe module was not found in the parent process")


def _validate_game_allocator(kernel32, process, game_base: int) -> tuple[int, int]:
    """Verify exact native allocator/free prologues before calling either routine."""
    realloc_fn = int(game_base) + _GAME_REALLOC_RVA
    free_fn = int(game_base) + _GAME_FREE_RVA
    if _read_exact(kernel32, process, realloc_fn, len(_REALLOC_PREFIX)) != _REALLOC_PREFIX:
        raise RuntimeError("LET IT DIE realloc signature does not match the validated executable")
    if _read_exact(kernel32, process, free_fn, len(_FREE_PREFIX)) != _FREE_PREFIX:
        raise RuntimeError("LET IT DIE free signature does not match the validated executable")
    return realloc_fn, free_fn


def _build_remote_call_stub(
    *, function: int, arg1: int, arg2: int, arg3: int, result_address: int
) -> bytes:
    """Build a Windows-x64 call trampoline storing RAX in remote result memory."""
    return b"".join(
        (
            b"\x48\x83\xEC\x28",                         # sub rsp, 0x28
            b"\x48\xB9" + struct.pack("<Q", int(arg1)),  # mov rcx, arg1
            b"\x48\xBA" + struct.pack("<Q", int(arg2)),  # mov rdx, arg2
            b"\x49\xB8" + struct.pack("<Q", int(arg3)),  # mov r8,  arg3
            b"\x48\xB8" + struct.pack("<Q", int(function)),
            b"\xFF\xD0",                                  # call rax
            b"\x49\xBB" + struct.pack("<Q", int(result_address)),
            b"\x49\x89\x03",                            # mov [r11], rax
            b"\x48\x83\xC4\x28",                       # add rsp, 0x28
            b"\x33\xC0",                                  # xor eax,eax
            b"\xC3",                                        # ret
        )
    )


def _wait_remote_completion(kernel32, process, thread) -> bool:
    """Never free executing code on a timeout. False means parent terminated."""
    while True:
        result = int(kernel32.WaitForSingleObject(thread, 100))
        if result == _WAIT_OBJECT_0:
            return True
        if result != _WAIT_TIMEOUT:
            raise RuntimeError(f"Remote thread status is indeterminate: 0x{result:X}")
        parent = int(kernel32.WaitForSingleObject(process, 0))
        if parent == _WAIT_OBJECT_0:
            return False
        if parent != _WAIT_TIMEOUT:
            raise RuntimeError(f"Parent status is indeterminate: 0x{parent:X}")


def _remote_call3(
    kernel32,
    process,
    *,
    function: int,
    arg1: int = 0,
    arg2: int = 0,
    arg3: int = 0,
    control: RebuildControl | None = None,
) -> int:
    """Call a proven native function inside LET IT DIE and return its RAX value."""
    block_size = 0x100
    remote = kernel32.VirtualAllocEx(
        process, None, block_size, _MEM_COMMIT | _MEM_RESERVE, _PAGE_EXECUTE_READWRITE
    )
    if not remote:
        raise _win_error("VirtualAllocEx failed for native-call trampoline")
    remote_addr = int(remote)
    result_addr = remote_addr + 0xE0
    safe_to_free = True
    try:
        _write_exact(kernel32, process, result_addr, b"\0" * 8)
        stub = _build_remote_call_stub(
            function=int(function),
            arg1=int(arg1),
            arg2=int(arg2),
            arg3=int(arg3),
            result_address=result_addr,
        )
        if len(stub) >= 0xE0:
            raise RuntimeError("native-call trampoline unexpectedly exceeds reserved code area")
        _write_exact(kernel32, process, remote_addr, stub)

        tid = wintypes.DWORD(0)
        thread = kernel32.CreateRemoteThread(
            process, None, 0, ctypes.c_void_p(remote_addr), None, 0, ctypes.byref(tid)
        )
        if not thread:
            raise _win_error("CreateRemoteThread failed for LET IT DIE allocator call")
        safe_to_free = False
        if control is not None:
            control.remote_started()
        try:
            completed = _wait_remote_completion(kernel32, process, thread)
            safe_to_free = True
            if not completed:
                raise RuntimeError("Parent terminated during allocator call")
        finally:
            if control is not None:
                control.remote_finished(safe_to_free)
            kernel32.CloseHandle(thread)

        return struct.unpack("<Q", _read_exact(kernel32, process, result_addr, 8))[0]
    finally:
        if safe_to_free:
            kernel32.VirtualFreeEx(process, ctypes.c_void_p(remote_addr), 0, _MEM_RELEASE)



def _build_resize_stub(
    *, record: int, target: int, old_num: int, realloc_fn: int, result_address: int, empty_word: int
) -> bytes:
    """Build one remote routine that reallocates and commits Data/Num/Max atomically."""
    requested_bytes = int(target) * _DEATH_BAG_ENTRY_SIZE
    grow_count = max(0, int(target) - int(old_num))
    grow_bytes = grow_count * _DEATH_BAG_ENTRY_SIZE

    code = bytearray()
    code += b"\x48\x83\xEC\x28"  # sub rsp,0x28
    code += b"\x48\xB8" + struct.pack("<Q", int(record))  # mov rax,record
    code += b"\x48\x8B\x88" + struct.pack("<I", _DEATH_BAG_ARRAY_OFFSET)  # mov rcx,[rax+18c]
    code += b"\xBA" + struct.pack("<I", requested_bytes)  # mov edx,bytes
    code += b"\x41\xB8\x08\x00\x00\x00"  # mov r8d,8
    code += b"\x48\xB8" + struct.pack("<Q", int(realloc_fn))
    code += b"\xFF\xD0"  # call rax
    code += b"\x48\x85\xC0"  # test rax,rax

    # jz failure; patch rel32 after body is assembled.
    jz_pos = len(code)
    code += b"\x0F\x84\x00\x00\x00\x00"
    code += b"\x49\x89\xC2"  # mov r10,rax (new Data)

    if grow_bytes:
        code += b"\x4D\x89\xD3"  # mov r11,r10
        if old_num * _DEATH_BAG_ENTRY_SIZE:
            code += b"\x49\x81\xC3" + struct.pack(
                "<I", int(old_num) * _DEATH_BAG_ENTRY_SIZE
            )
        code += b"\xB9" + struct.pack("<I", grow_bytes)  # mov ecx,grow_bytes
        code += b"\x31\xC0"  # xor eax,eax
        zero_loop = len(code)
        code += b"\x41\x88\x03"  # mov byte ptr [r11],al
        code += b"\x49\xFF\xC3"  # inc r11
        code += b"\xFF\xC9"  # dec ecx
        zero_rel = zero_loop - (len(code) + 2)
        code += b"\x75" + struct.pack("b", zero_rel)

        if int(empty_word) & 0xFFFFFFFF:
            code += b"\x4C\x89\xD2"  # mov rdx,r10
            if old_num * _DEATH_BAG_ENTRY_SIZE:
                code += b"\x48\x81\xC2" + struct.pack(
                    "<I", int(old_num) * _DEATH_BAG_ENTRY_SIZE
                )
            code += b"\xB8" + struct.pack("<I", int(empty_word) & 0xFFFFFFFF)
            code += b"\xB9" + struct.pack("<I", grow_count)
            loop_start = len(code)
            code += b"\x89\x02"  # mov [rdx],eax
            code += b"\x48\x83\xC2\x44"  # add rdx,0x44
            code += b"\xFF\xC9"  # dec ecx
            rel = loop_start - (len(code) + 2)
            if not -128 <= rel <= 127:
                raise RuntimeError("internal V0.88 slot-init loop exceeded short-jump range")
            code += b"\x75" + struct.pack("b", rel)  # jnz loop_start

    # Commit the complete TArray header before the remote thread returns.
    code += b"\x48\xB8" + struct.pack("<Q", int(record))
    code += b"\x4C\x89\x90" + struct.pack("<I", _DEATH_BAG_ARRAY_OFFSET)
    code += b"\xC7\x80" + struct.pack("<I", _DEATH_BAG_NUM_OFFSET) + struct.pack("<I", int(target))
    code += b"\xC7\x80" + struct.pack("<I", _DEATH_BAG_MAX_OFFSET) + struct.pack("<I", int(target))
    code += b"\x49\xBB" + struct.pack("<Q", int(result_address))
    code += b"\x4D\x89\x13"  # mov [r11],r10
    code += b"\x48\x83\xC4\x28\x33\xC0\xC3"

    failure_pos = len(code)
    code += b"\x49\xBB" + struct.pack("<Q", int(result_address))
    code += b"\x49\xC7\x03\x00\x00\x00\x00"  # result=0
    code += b"\x48\x83\xC4\x28\x33\xC0\xC3"

    rel32 = failure_pos - (jz_pos + 6)
    struct.pack_into("<i", code, jz_pos + 2, rel32)
    return bytes(code)


def _remote_resize_death_bag(
    kernel32, process, *, record: int, target: int, old_num: int, realloc_fn: int, empty_word: int,
    control: RebuildControl | None = None,
) -> int:
    """Realloc the real buffer and commit its TArray header inside one remote call."""
    block_size = 0x300
    remote = kernel32.VirtualAllocEx(
        process, None, block_size, _MEM_COMMIT | _MEM_RESERVE, _PAGE_EXECUTE_READWRITE
    )
    if not remote:
        raise _win_error("VirtualAllocEx failed for Death Bag resize trampoline")
    remote_addr = int(remote)
    result_addr = remote_addr + 0x2E0
    safe_to_free = True
    try:
        _write_exact(kernel32, process, result_addr, b"\0" * 8)
        stub = _build_resize_stub(
            record=int(record), target=int(target), old_num=int(old_num),
            realloc_fn=int(realloc_fn), result_address=result_addr, empty_word=int(empty_word),
        )
        if len(stub) >= 0x2E0:
            raise RuntimeError("Death Bag resize trampoline exceeds reserved code area")
        _write_exact(kernel32, process, remote_addr, stub)
        tid = wintypes.DWORD(0)
        thread = kernel32.CreateRemoteThread(
            process, None, 0, ctypes.c_void_p(remote_addr), None, 0, ctypes.byref(tid)
        )
        if not thread:
            raise _win_error("CreateRemoteThread failed for Death Bag resize")
        safe_to_free = False
        if control is not None:
            control.remote_started()
        try:
            completed = _wait_remote_completion(kernel32, process, thread)
            safe_to_free = True
            if not completed:
                raise RuntimeError("Parent terminated during Death Bag resize")
        finally:
            if control is not None:
                control.remote_finished(safe_to_free)
            kernel32.CloseHandle(thread)
        return struct.unpack("<Q", _read_exact(kernel32, process, result_addr, 8))[0]
    finally:
        if safe_to_free:
            kernel32.VirtualFreeEx(process, ctypes.c_void_p(remote_addr), 0, _MEM_RELEASE)

def _read_tarray_header(kernel32, process, record: int) -> tuple[int, int, int]:
    raw = _read_exact(kernel32, process, int(record) + _DEATH_BAG_ARRAY_OFFSET, 16)
    data, num, maximum = struct.unpack("<Qii", raw)
    if num < 0 or maximum < 0 or num > _MAX_REASONABLE_SLOTS or maximum > _MAX_REASONABLE_SLOTS:
        raise RuntimeError(f"implausible Death Bag TArray bounds Num={num} Max={maximum}")
    if maximum < num:
        raise RuntimeError(f"invalid Death Bag TArray Max={maximum} < Num={num}")
    if num and not data:
        raise RuntimeError("Death Bag has nonzero Num with null Data")
    return int(data), int(num), int(maximum)


def _parse_slot_fstrings(slot: bytes) -> list[tuple[int, int, int]]:
    if len(slot) != _DEATH_BAG_ENTRY_SIZE:
        raise RuntimeError("Death Bag slot read has an invalid size")
    fields: list[tuple[int, int, int]] = []
    for offset in _DEATH_BAG_FSTRING_OFFSETS:
        data, num, maximum = struct.unpack_from("<Qii", slot, offset)
        if num < 0 or maximum < 0 or maximum < num or maximum > _MAX_REASONABLE_STRING_CAPACITY:
            raise RuntimeError(
                f"implausible FString at slot+0x{offset:X}: Data=0x{data:X} Num={num} Max={maximum}"
            )
        if num and not data:
            raise RuntimeError(
                f"FString at slot+0x{offset:X} has Num={num} with null Data"
            )
        fields.append((int(data), int(num), int(maximum)))
    return fields


def _slot_is_empty(slot: bytes, kernel32, process) -> bool:
    """Mirror the native free-slot convention used on the slot's +0x24 FString.

    The validated native helper treats a zero/one-character FString or a value
    beginning with '-' as an unused/invalid item identifier.
    """
    data, num, _maximum = struct.unpack_from("<Qii", slot, 0x24)
    if num <= 1:
        return True
    if not data:
        return True
    first = _read_exact(kernel32, process, int(data), 2)
    return struct.unpack("<H", first)[0] == ord("-")


def _resolve_persistent_chara(kernel32, process, userdata: int, active_index: int) -> int:
    """Use the exact persistent Chara path already proven in V0.82 native code."""
    if active_index < 0:
        return 0
    payload = struct.unpack(
        "<Q", _read_exact(kernel32, process, int(userdata) + _USERDATA_PAYLOAD_OFFSET, 8)
    )[0]
    if not payload:
        return 0
    count = struct.unpack(
        "<i", _read_exact(kernel32, process, int(payload) + _PAYLOAD_CHARA_NUM_OFFSET, 4)
    )[0]
    array_base = struct.unpack(
        "<Q", _read_exact(kernel32, process, int(payload) + _PAYLOAD_CHARA_DATA_OFFSET, 8)
    )[0]
    if count <= 0 or count > 64 or active_index >= count or not array_base:
        return 0
    return int(array_base) + int(active_index) * _BRGT_DBCHARA_SIZE


def _rebuild_death_bag_record(
    kernel32,
    process,
    *,
    record: int,
    target: int,
    realloc_fn: int,
    free_fn: int,
    control: RebuildControl | None = None,
) -> dict[str, int]:
    """Grow a Death Bag without removing contents; native ownership handles shrink."""
    if not 1 <= int(target) <= _MAX_REASONABLE_SLOTS:
        raise RuntimeError(f"requested Death Bag capacity is outside 1-{_MAX_REASONABLE_SLOTS}: {target}")

    old_data, old_num, old_max = _read_tarray_header(kernel32, process, record)
    target = int(target)

    # This legacy worker receives only starting_death_bag, not the progressive
    # target. Shrinking here could remove acquired slots and their contents
    # before native ownership is established. Leave the complete array intact;
    # the native transaction knows the real target and rejects occupied tails.
    if target < old_num:
        return {
            "old_data": old_data, "old_num": old_num, "old_max": old_max,
            "new_data": old_data, "new_num": old_num, "new_max": old_max,
            "freed_strings": 0, "already": 0, "deferred_shrink": 1,
        }

    if old_num == target and old_max == target:
        return {
            "old_data": old_data, "old_num": old_num, "old_max": old_max,
            "new_data": old_data, "new_num": old_num, "new_max": old_max,
            "freed_strings": 0, "already": 1,
        }

    slots: list[bytes] = []
    if old_num:
        raw = _read_exact(kernel32, process, old_data, old_num * _DEATH_BAG_ENTRY_SIZE)
        slots = [
            raw[i * _DEATH_BAG_ENTRY_SIZE : (i + 1) * _DEATH_BAG_ENTRY_SIZE]
            for i in range(old_num)
        ]
        # Validate every proven Death Bag element before any destructive action.
        for slot in slots:
            _parse_slot_fstrings(slot)

    # Preserve pointers owned by retained slots so even an unexpected shared
    # FString allocation can never be freed while still referenced.
    retained_pointers: set[int] = set()
    for slot in slots[: min(target, old_num)]:
        for ptr, _num, _maximum in _parse_slot_fstrings(slot):
            if ptr:
                retained_pointers.add(ptr)

    removed_pointers: list[int] = []
    if target < old_num:
        seen: set[int] = set()
        for slot in slots[target:]:
            for ptr, _num, _maximum in _parse_slot_fstrings(slot):
                if ptr and ptr not in retained_pointers and ptr not in seen:
                    seen.add(ptr)
                    removed_pointers.append(ptr)

    # Capture the scalar word used by a proven empty slot before the buffer can
    # move.  It is only needed for future growth; all FString fields are zeroed.
    empty_word = 0
    if target > old_num:
        empty_template_found = False
        for slot in slots:
            if _slot_is_empty(slot, kernel32, process):
                empty_word = struct.unpack_from("<I", slot, 0)[0]
                empty_template_found = True
                break
        if not empty_template_found:
            raise RuntimeError(
                "Death Bag growth refused: no proven native empty-slot template is available; "
                "V0.88 will not invent an entry layout"
            )

    # Realloc + Data/Num/Max commit happen inside ONE remote thread.  There is
    # no interval where BrgDbChara points at a buffer already moved by realloc.
    new_data = _remote_resize_death_bag(
        kernel32, process, record=record, target=target, old_num=old_num,
        realloc_fn=realloc_fn, empty_word=empty_word,
        control=control,
    )
    if not new_data:
        raise RuntimeError(
            f"LET IT DIE realloc returned null for {target} Death Bag slots "
            f"({target * _DEATH_BAG_ENTRY_SIZE} bytes); original array left unchanged"
        )

    # The removed outer slots no longer exist.  Release only their nested
    # FString allocations, using LET IT DIE's own allocator and never freeing a
    # pointer still referenced by a retained slot.
    freed_pointers: set[int] = set()
    for ptr in removed_pointers:
        _remote_call3(kernel32, process, function=free_fn, arg1=ptr, control=control)
        freed_pointers.add(ptr)

    verify_data, verify_num, verify_max = _read_tarray_header(kernel32, process, record)
    if (verify_data, verify_num, verify_max) != (int(new_data), target, target):
        raise RuntimeError(
            "Death Bag rebuild verification failed: "
            f"Data=0x{verify_data:X} Num={verify_num} Max={verify_max}"
        )

    return {
        "old_data": old_data, "old_num": old_num, "old_max": old_max,
        "new_data": verify_data, "new_num": verify_num, "new_max": verify_max,
        "freed_strings": len(freed_pointers), "already": 0,
    }


def _hold_until_parent_exit(kernel32, process) -> None:
    """Keep the runtime mutex owned when an old remote thread is uncertain."""
    while int(kernel32.WaitForSingleObject(process, 100)) != _WAIT_OBJECT_0:
        time.sleep(0.05)


def _acquire_runtime_mutex(kernel32, process, parent_pid: int, control: RebuildControl):
    created, exited, kernel, user = (wintypes.FILETIME() for _ in range(4))
    if not kernel32.GetProcessTimes(process, ctypes.byref(created), ctypes.byref(exited),
                                    ctypes.byref(kernel), ctypes.byref(user)):
        raise _win_error("GetProcessTimes failed for runtime ownership")
    creation = (created.dwHighDateTime << 32) | created.dwLowDateTime
    if not control.bind_parent(parent_pid, creation):
        raise RuntimeError("Parent process identity changed")
    mutex = kernel32.CreateMutexW(None, False, runtime_mutex_name(parent_pid, creation))
    if not mutex:
        raise _win_error("CreateMutexW failed for runtime ownership")
    try:
        while not control.stop.is_set():
            result = int(kernel32.WaitForSingleObject(mutex, 100))
            if result == _WAIT_OBJECT_0:
                return mutex, True
            if result == 0x80:  # WAIT_ABANDONED: a previous writer may have died mid-call.
                control.mark_unsafe()
                _hold_until_parent_exit(kernel32, process)
                return mutex, True
            if result != _WAIT_TIMEOUT:
                raise RuntimeError("Runtime mutex status is indeterminate")
            if int(kernel32.WaitForSingleObject(process, 0)) == _WAIT_OBJECT_0:
                break
        return mutex, False
    except BaseException:
        kernel32.CloseHandle(mutex)
        raise


def _watch_and_rebuild_blocking(
    *, bridge_dir: Path, parent_pid: int, ap_capacity: int, control: RebuildControl
) -> None:
    """Watch proven NON_TITLE records and rebuild each new active Death Bag once."""
    if sys.platform != "win32":
        _append_bridge_log(bridge_dir, "V0.88 DEATH BAG: full rebuild skipped outside Windows.")
        return
    if int(parent_pid) <= 0:
        _append_bridge_log(bridge_dir, "V0.88 DEATH BAG: invalid parent PID; no rebuild attempted.")
        return
    if not 1 <= int(ap_capacity) <= _MAX_REASONABLE_SLOTS:
        _append_bridge_log(
            bridge_dir,
            f"V0.88 DEATH BAG: invalid AP capacity {ap_capacity}; no rebuild attempted.",
        )
        return

    kernel32 = _configure_kernel32()
    process = kernel32.OpenProcess(_PROCESS_RIGHTS, False, int(parent_pid))
    if not process:
        _append_bridge_log(
            bridge_dir,
            f"V0.88 DEATH BAG: OpenProcess({parent_pid}) failed; Win32={ctypes.get_last_error()}.",
        )
        return

    mod_log = bridge_dir / "mod.log"
    try:
        offset = mod_log.stat().st_size
    except FileNotFoundError:
        offset = 0
    pending = ""
    runtime_mutex = None
    owns_runtime_mutex = False
    try:
        runtime_mutex, owns_runtime_mutex = _acquire_runtime_mutex(kernel32, process, int(parent_pid), control)
        if not owns_runtime_mutex or control.stop.is_set():
            return
        game_base = _find_game_module_base(kernel32, int(parent_pid))
        realloc_fn, free_fn = _validate_game_allocator(kernel32, process, game_base)
        _append_bridge_log(
            bridge_dir,
            "AP ITEMS: legacy Death Bag watcher armed after authentication; shared writer mutex held; "
            f"waiting for NON_TITLE; AP={int(ap_capacity)} game=0x{game_base:X}.",
        )

        while not control.stop.is_set() and _process_is_alive(kernel32, process):
            try:
                with mod_log.open("r", encoding="utf-8", errors="replace") as stream:
                    stream.seek(offset)
                    new_text = stream.read()
                    offset = stream.tell()
            except FileNotFoundError:
                new_text = ""

            if new_text:
                pending += new_text
                lines = pending.splitlines(keepends=True)
                pending = ""
                if lines and not lines[-1].endswith(("\n", "\r")):
                    pending = lines.pop()

                for raw_line in lines:
                    if control.stop.is_set():
                        return
                    match = _NON_TITLE_RE.search(raw_line.rstrip("\r\n"))
                    if not match:
                        continue

                    userdata = int(match.group(1), 16)
                    body = int(match.group(2), 16)
                    active_index = int(match.group(3))

                    records: list[tuple[str, int]] = []
                    try:
                        persistent = _resolve_persistent_chara(
                            kernel32, process, userdata, active_index
                        )
                    except Exception as exc:
                        persistent = 0
                        _append_bridge_log(
                            bridge_dir,
                            "V0.88 DEATH BAG: persistent Chara resolution failed; "
                            f"{type(exc).__name__}: {exc}",
                        )

                    if persistent:
                        records.append(("persistent Chara", persistent))
                    if body and body != persistent:
                        records.append(("runtime Body", body))
                    elif body and not records:
                        records.append(("runtime Body", body))

                    for label, record in records:
                        if control.stop.is_set():
                            return
                        try:
                            result = _rebuild_death_bag_record(
                                kernel32,
                                process,
                                record=record,
                                target=int(ap_capacity),
                                realloc_fn=realloc_fn,
                                free_fn=free_fn,
                                control=control,
                            )
                        except Exception as exc:
                            _append_bridge_log(
                                bridge_dir,
                                f"V0.88 DEATH BAG: {label} rebuild failed record=0x{record:X}: "
                                f"{type(exc).__name__}: {exc}",
                            )
                            continue

                        if result.get("deferred_shrink"):
                            _append_bridge_log(
                                bridge_dir,
                                f"AP INVENTORY R26B: {label} base-only shrink deferred "
                                f"{result['old_num']}->{int(ap_capacity)}; contents unchanged; "
                                "final capacity belongs to the native progressive transaction.",
                            )
                        elif result["already"]:
                            _append_bridge_log(
                                bridge_dir,
                                f"V0.88 DEATH BAG: {label} record=0x{record:X} already fully "
                                f"rebuilt Data=0x{result['new_data']:X} Num/Max={result['new_num']}/{result['new_max']}.",
                            )
                        else:
                            _append_bridge_log(
                                bridge_dir,
                                f"V0.88 DEATH BAG REBUILT: {label} record=0x{record:X} "
                                f"Data 0x{result['old_data']:X}->0x{result['new_data']:X} "
                                f"Num {result['old_num']}->{result['new_num']} "
                                f"Max {result['old_max']}->{result['new_max']} "
                                f"freed_strings={result['freed_strings']} target={int(ap_capacity)}.",
                            )

            time.sleep(0.10)

    except Exception as exc:
        _append_bridge_log(
            bridge_dir,
            f"V0.88 DEATH BAG: runtime rebuild watcher stopped: {type(exc).__name__}: {exc}",
        )
    finally:
        if runtime_mutex is not None:
            if owns_runtime_mutex:
                if control.unsafe:
                    _append_bridge_log(bridge_dir, "AP ITEMS: uncertain remote call; keeping runtime ownership until parent exit; no handoff.")
                    _hold_until_parent_exit(kernel32, process)
                kernel32.ReleaseMutex(runtime_mutex)
            kernel32.CloseHandle(runtime_mutex)
        kernel32.CloseHandle(process)


async def watch_and_rebuild_death_bag(
    *, bridge_dir: Path, parent_pid: int, ap_capacity: int, control: RebuildControl | None = None
) -> None:
    """Rebuild the real Death Bag after NON_TITLE without blocking AP networking."""
    owner = control if control is not None else RebuildControl()
    def run() -> None:
        owner.worker_started()
        try:
            _watch_and_rebuild_blocking(bridge_dir=Path(bridge_dir), parent_pid=int(parent_pid),
                                       ap_capacity=int(ap_capacity), control=owner)
        finally:
            # This runs in the real worker thread, never an asyncio cancellation.
            owner.worker_finished()
    await asyncio.to_thread(run)
