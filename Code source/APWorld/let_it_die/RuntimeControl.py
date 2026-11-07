"""Cooperative ownership handoff; an asyncio cancellation is never completion."""
from __future__ import annotations

import threading
import os


class RebuildControl:
    def __init__(self) -> None:
        self.stop = threading.Event()
        self.finished = threading.Event()
        self.started = threading.Event()
        self._lock = threading.Lock()
        self._in_flight = 0
        self._unsafe = False
        self.parent_pid = 0
        self.creation_time = 0

    def bind_parent(self, pid: int, creation_time: int) -> bool:
        with self._lock:
            if (not pid or not creation_time or (self.parent_pid and
                    (pid, creation_time) != (self.parent_pid, self.creation_time))):
                self._unsafe = True
                self.stop.set()
                return False
            self.parent_pid, self.creation_time = pid, creation_time
            return True

    def mark_unsafe(self) -> None:
        with self._lock:
            self._unsafe = True
            self.stop.set()

    def request_stop(self) -> None:
        self.stop.set()

    def worker_started(self) -> None:
        with self._lock:
            if self.started.is_set():
                raise RuntimeError("A rebuild controller cannot own two workers")
            self.started.set()

    def worker_finished(self) -> None:
        self.finished.set()

    def remote_started(self) -> None:
        with self._lock:
            self._in_flight += 1

    def remote_finished(self, confirmed: bool) -> None:
        with self._lock:
            if self._in_flight <= 0:
                raise RuntimeError("Unbalanced rebuild remote-call tracking")
            self._in_flight -= 1
            if not confirmed:
                self._unsafe = True
                self.stop.set()

    @property
    def unsafe(self) -> bool:
        with self._lock:
            return self._unsafe

    @property
    def quiesced(self) -> bool:
        with self._lock:
            return (self.stop.is_set() and self.started.is_set() and self.finished.is_set()
                    and self._in_flight == 0 and not self._unsafe)


def runtime_mutex_name(pid: int, creation_time: int) -> str:
    if type(pid) is not int or not 0 < pid <= 0xffffffff or type(creation_time) is not int or not 0 < creation_time <= 0xffffffffffffffff:
        raise ValueError("Invalid runtime process identity")
    return f"Local\\LID_AP_CHARACTER_{pid:08X}_{creation_time:016X}"


def query_parent_creation_time(pid: int) -> int:
    """Read process creation FILETIME only; no remote allocation/calls/writes."""
    if os.name != "nt":
        return 0
    import ctypes
    from ctypes import wintypes as w
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    k.OpenProcess.restype = w.HANDLE
    k.GetProcessTimes.argtypes = [w.HANDLE, ctypes.POINTER(w.FILETIME), ctypes.POINTER(w.FILETIME),
                                 ctypes.POINTER(w.FILETIME), ctypes.POINTER(w.FILETIME)]
    k.GetProcessTimes.restype = w.BOOL
    k.CloseHandle.argtypes = [w.HANDLE]
    k.CloseHandle.restype = w.BOOL
    process = k.OpenProcess(0x1000, False, pid)
    if not process:
        return 0
    try:
        created, exited, kernel, user = w.FILETIME(), w.FILETIME(), w.FILETIME(), w.FILETIME()
        if not k.GetProcessTimes(process, ctypes.byref(created), ctypes.byref(exited),
                                 ctypes.byref(kernel), ctypes.byref(user)):
            return 0
        return (created.dwHighDateTime << 32) | created.dwLowDateTime
    finally:
        k.CloseHandle(process)
