from __future__ import annotations

import ctypes
import os
from pathlib import Path


NAMUMARK_OUTPUT_HTML = 0
NAMUMARK_OUTPUT_AST_JSON = 1
NAMUMARK_OK = 0


class NamumarkBuffer(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_size_t)]


def _default_library_path() -> Path:
    root = Path(__file__).resolve().parents[2]
    candidates = [
        root / "build" / "libnamumark.dylib",
        root / "build" / "libnamumark.so",
        root / "build" / "namumark.dll",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _load_library(path: str | os.PathLike[str] | None = None) -> ctypes.CDLL:
    lib = ctypes.CDLL(str(path or os.environ.get("NAMUMARK_LIBRARY") or _default_library_path()))
    lib.namumark_render.argtypes = [
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.c_int,
        ctypes.POINTER(NamumarkBuffer),
    ]
    lib.namumark_render.restype = ctypes.c_int
    lib.namumark_buffer_free.argtypes = [ctypes.POINTER(NamumarkBuffer)]
    lib.namumark_buffer_free.restype = None
    lib.namumark_status_message.argtypes = [ctypes.c_int]
    lib.namumark_status_message.restype = ctypes.c_char_p
    return lib


class Namumark:
    def __init__(self, library_path: str | os.PathLike[str] | None = None) -> None:
        self._lib = _load_library(library_path)

    def render(self, text: str | bytes, output_format: int = NAMUMARK_OUTPUT_HTML) -> str:
        data = text.encode("utf-8") if isinstance(text, str) else text
        output = NamumarkBuffer()
        status = self._lib.namumark_render(data, len(data), output_format, ctypes.byref(output))
        if status != NAMUMARK_OK:
            message = self._lib.namumark_status_message(status).decode("utf-8")
            raise RuntimeError(message)
        try:
            raw = ctypes.string_at(output.data, output.size)
            return raw.decode("utf-8")
        finally:
            self._lib.namumark_buffer_free(ctypes.byref(output))

    def render_html(self, text: str | bytes) -> str:
        return self.render(text, NAMUMARK_OUTPUT_HTML)

    def render_ast_json(self, text: str | bytes) -> str:
        return self.render(text, NAMUMARK_OUTPUT_AST_JSON)


_default = None


def _client() -> Namumark:
    global _default
    if _default is None:
        _default = Namumark()
    return _default


def render_html(text: str | bytes) -> str:
    return _client().render_html(text)


def render_ast_json(text: str | bytes) -> str:
    return _client().render_ast_json(text)
