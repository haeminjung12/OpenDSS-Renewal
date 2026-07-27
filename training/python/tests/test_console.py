from __future__ import annotations

import io
import json
import sys
import unittest

from droplet_trainer.console import make_unicode_safe_stream
from droplet_trainer.train import JsonlEmitter


class ConsoleSafetyTests(unittest.TestCase):
    def test_make_unicode_safe_stream_backslash_escapes_unencodable_output(self) -> None:
        buffer = io.BytesIO()
        stream = io.TextIOWrapper(buffer, encoding="cp1252", errors="strict", write_through=True)

        safe_stream = make_unicode_safe_stream(stream)
        safe_stream.write("export ✅ complete\n")
        safe_stream.flush()

        self.assertEqual(buffer.getvalue().decode("cp1252").splitlines(), ["export \\u2705 complete"])

    def test_jsonl_emitter_remains_valid_json_on_charmap_stdout(self) -> None:
        buffer = io.BytesIO()
        stream = io.TextIOWrapper(buffer, encoding="cp1252", errors="strict", write_through=True)
        safe_stream = make_unicode_safe_stream(stream)
        original_stdout = sys.stdout
        try:
            sys.stdout = safe_stream
            JsonlEmitter("train", "run_unicode").emit("progress", message="export ✅ complete")
        finally:
            sys.stdout = original_stdout

        payload = json.loads(buffer.getvalue().decode("cp1252"))
        self.assertEqual(payload["event"], "progress")
        self.assertEqual(payload["message"], "export ✅ complete")


if __name__ == "__main__":
    unittest.main()
