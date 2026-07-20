from __future__ import annotations

import pytest

from droplet_trainer.publication_experiments.latency_benchmark import latency_statistics


def test_latency_statistics_are_complete_and_deterministic():
    stats = latency_statistics([1.0, 2.0, 3.0, 4.0])
    assert stats["iterations"] == 4
    assert stats["median_ms"] == 2.5
    assert stats["p95_ms"] == pytest.approx(3.85)
    assert stats["mean_ms"] == 2.5
    assert stats["std_ms"] == pytest.approx(1.11803398875)
    assert stats["throughput_per_second"] == 400.0


def test_latency_statistics_reject_empty_samples():
    with pytest.raises(ValueError, match="cannot be empty"):
        latency_statistics([])
