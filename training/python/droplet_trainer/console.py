from __future__ import annotations

import io
import locale
import sys
from typing import TextIO


UNICODE_SAFE_ERRORS = "backslashreplace"


def make_unicode_safe_stream(stream: TextIO) -> TextIO:
    reconfigure = getattr(stream, "reconfigure", None)
    if callable(reconfigure):
        try:
            reconfigure(errors=UNICODE_SAFE_ERRORS)
            return stream
        except (AttributeError, OSError, ValueError):
            pass

    buffer = getattr(stream, "buffer", None)
    if buffer is None:
        return stream

    encoding = getattr(stream, "encoding", None) or locale.getpreferredencoding(False) or "utf-8"
    line_buffering = bool(getattr(stream, "line_buffering", False))
    write_through = bool(getattr(stream, "write_through", False))
    try:
        return io.TextIOWrapper(
            buffer,
            encoding=encoding,
            errors=UNICODE_SAFE_ERRORS,
            newline=None,
            line_buffering=line_buffering,
            write_through=write_through,
        )
    except Exception:
        return stream


def configure_console_io() -> None:
    sys.stdout = make_unicode_safe_stream(sys.stdout)
    sys.stderr = make_unicode_safe_stream(sys.stderr)
