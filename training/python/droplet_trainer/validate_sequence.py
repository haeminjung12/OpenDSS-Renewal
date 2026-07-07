from __future__ import annotations

import csv
import json
from pathlib import Path
from typing import Any

from .dataset import utc_now, write_csv
from .errors import EXIT_SCHEMA_MISMATCH
from .schema import ClassSchema


def _parse_int(value: str | None) -> int | None:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    return int(text)


def _parse_bool(value: str | None) -> bool | None:
    if value is None:
        return None
    text = str(value).strip().lower()
    if text in {"1", "true", "yes"}:
        return True
    if text in {"0", "false", "no"}:
        return False
    return None


def _normalize_runtime_label(row: dict[str, str], schema: ClassSchema) -> tuple[str | None, str | None, str]:
    for field in ("predicted_class_id", "predicted_label"):
        raw = row.get(field, "")
        class_id, status = schema.normalize_label(raw)
        if class_id is not None:
            return class_id, raw, status
    predicted_index = _parse_int(row.get("predicted_index"))
    if predicted_index is not None and 0 <= predicted_index < len(schema.classes):
        class_id = schema.classes[predicted_index]
        return class_id, str(predicted_index), "index"
    return None, None, "unknown"


def _load_event_decisions(path: Path, schema: ClassSchema) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    runtime_rows: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    for row in rows:
        normalized_class_id, normalized_source, normalization_status = _normalize_runtime_label(row, schema)
        first_detected = _parse_int(row.get("first_detected_frame"))
        classification_frame = _parse_int(row.get("classification_frame"))
        motion_decision_frame = _parse_int(row.get("motion_decision_frame"))
        runtime_rows.append(
            {
                "runtime_event_id": row.get("event_id", ""),
                "schema_version": row.get("schema_version", ""),
                "source_sequence_id": row.get("source_sequence_id", ""),
                "first_detected_frame": first_detected,
                "classification_frame": classification_frame,
                "motion_decision_frame": motion_decision_frame,
                "frames_tracked": _parse_int(row.get("frames_tracked")),
                "predicted_index": _parse_int(row.get("predicted_index")),
                "predicted_class_id_raw": row.get("predicted_class_id", ""),
                "predicted_label_raw": row.get("predicted_label", ""),
                "predicted_class_id": normalized_class_id,
                "predicted_display_label": schema.display_labels.get(normalized_class_id or "", normalized_class_id or ""),
                "normalized_source": normalized_source,
                "normalization_status": normalization_status,
                "should_trigger": _parse_bool(row.get("should_trigger")),
                "trigger_attempted": _parse_bool(row.get("trigger_attempted")),
                "trigger_ok": _parse_bool(row.get("trigger_ok")),
                "motion_decision_dir": row.get("motion_decision_dir", ""),
                "source_frame_filename": row.get("source_frame_filename", ""),
                "crop_path": row.get("crop_path", ""),
                "confidence": row.get("confidence", ""),
            }
        )
        if normalized_class_id is None:
            warnings.append(
                {
                    "code": "UNMAPPED_RUNTIME_LABEL",
                    "message": "Runtime event row could not be normalized into the configured class schema.",
                    "details": {
                        "event_id": row.get("event_id", ""),
                        "predicted_class_id": row.get("predicted_class_id", ""),
                        "predicted_label": row.get("predicted_label", ""),
                    },
                }
            )
    return runtime_rows, warnings


def _load_ground_truth(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    events = payload.get("events")
    if not isinstance(events, list):
        raise ValueError("sequence_ground_truth.json is missing the events list.")
    return payload, events


def _load_review_checklist(path: Path | None) -> tuple[dict[str, Any], dict[str, Any], list[dict[str, Any]], list[dict[str, Any]]]:
    if path is None:
        return {}, {}, [], []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))

    accepted_missed: dict[str, Any] = {}
    accepted_runtime: dict[str, Any] = {}
    warnings: list[dict[str, Any]] = []
    accepted_actions = {"accept_gt_event_no_runtime_available", "detector_false_positive"}

    for row in rows:
        action = (row.get("reviewer_action") or "").strip()
        review_item_id = (row.get("review_item_id") or "").strip()
        if not action:
            continue
        if action not in accepted_actions:
            warnings.append(
                {
                    "code": "UNUSED_REVIEW_CHECKLIST_ACTION",
                    "message": "Review checklist row has an action that is not consumed by sequence adjudication reporting.",
                    "details": {"review_item_id": review_item_id, "reviewer_action": action},
                }
            )
            continue
        if action == "accept_gt_event_no_runtime_available":
            expected_event_id = (row.get("expected_event_id") or "").strip()
            if expected_event_id:
                accepted_missed[expected_event_id] = row
        elif action == "detector_false_positive":
            runtime_event_id = (row.get("runtime_event_id") or "").strip()
            if runtime_event_id:
                accepted_runtime[runtime_event_id] = row

    return accepted_missed, accepted_runtime, rows, warnings


def _in_window(frame: int | None, window: dict[str, Any]) -> bool:
    if frame is None:
        return False
    return int(window["start"]) <= frame <= int(window["end"])


def _interval_overlap(start_a: int | None, end_a: int | None, start_b: int, end_b: int) -> bool:
    if start_a is None and end_a is None:
        return False
    left = start_a if start_a is not None else end_a
    right = end_a if end_a is not None else start_a
    if left is None or right is None:
        return False
    if right < left:
        left, right = right, left
    return not (right < start_b or left > end_b)


def _match_basis(runtime_row: dict[str, Any], expected_event: dict[str, Any]) -> tuple[int, int, str]:
    decision_window = expected_event["expected_decision_frame_window"]
    event_window = expected_event["event_window_frames"]
    class_frame = runtime_row["classification_frame"]
    first_detected = runtime_row["first_detected_frame"]
    motion_frame = runtime_row["motion_decision_frame"]
    window_center = (int(decision_window["start"]) + int(decision_window["end"])) // 2
    reference_frame = class_frame if class_frame is not None else first_detected if first_detected is not None else motion_frame
    distance = abs((reference_frame if reference_frame is not None else window_center) - window_center)

    if _in_window(class_frame, decision_window):
        return 0, distance, "classification_frame_in_expected_decision_window"
    if _in_window(class_frame, event_window):
        return 1, distance, "classification_frame_in_event_window"
    if _in_window(first_detected, event_window):
        return 2, distance, "first_detected_frame_in_event_window"
    if _interval_overlap(first_detected, motion_frame, int(event_window["start"]), int(event_window["end"])):
        return 3, distance, "runtime_event_interval_overlaps_event_window"
    return 99, distance, "no_overlap"


def _nearest_expected(runtime_row: dict[str, Any], expected_events: list[dict[str, Any]]) -> tuple[str | None, int | None]:
    reference_frame = runtime_row["classification_frame"] or runtime_row["first_detected_frame"] or runtime_row["motion_decision_frame"]
    if reference_frame is None:
        return None, None
    best_id: str | None = None
    best_distance: int | None = None
    for expected_event in expected_events:
        window = expected_event["event_window_frames"]
        center = (int(window["start"]) + int(window["end"])) // 2
        distance = abs(reference_frame - center)
        if best_distance is None or distance < best_distance:
            best_id = str(expected_event.get("event_id"))
            best_distance = distance
    return best_id, best_distance


def _flatten_metrics(summary: dict[str, Any]) -> list[dict[str, str]]:
    raw_metrics = summary["ground_truth_metrics"]
    adjudicated_metrics = summary["adjudicated_ground_truth_metrics"]
    flat: list[dict[str, str]] = []
    for key in (
        "expected_events_total",
        "runtime_events_total",
        "matched_events",
        "missed_events",
        "extra_runtime_events",
        "class_matches",
        "class_mismatches",
        "trigger_matches",
        "trigger_mismatches",
        "decision_window_matches",
    ):
        flat.append({"metric": key, "raw_value": str(raw_metrics.get(key, "")), "adjudicated_value": str(adjudicated_metrics.get(key, raw_metrics.get(key, "")))})
    for key in ("class_accuracy_on_matched", "trigger_accuracy_on_matched"):
        raw_value = raw_metrics.get(key)
        adjudicated_value = adjudicated_metrics.get(key)
        flat.append(
            {
                "metric": key,
                "raw_value": "" if raw_value is None else f"{raw_value:.12g}",
                "adjudicated_value": "" if adjudicated_value is None else f"{adjudicated_value:.12g}",
            }
        )
    for key in (
        "accepted_detector_false_positive_runtime_events",
        "accepted_startup_no_runtime_events",
        "unresolved_missed_events",
        "unresolved_extra_runtime_events",
    ):
        flat.append({"metric": key, "raw_value": "", "adjudicated_value": str(adjudicated_metrics.get(key, ""))})
    return flat


def _coerce_metric_value(value: Any) -> Any:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    try:
        return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        return text


def _load_sequence_metrics_csv(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        return {
            "source": "sequence_metrics_csv",
            "csv_schema": "empty",
            "ground_truth_metrics": {},
            "adjudicated_ground_truth_metrics": {},
        }

    fieldnames = set(rows[0].keys())
    raw_metrics: dict[str, Any] = {}
    adjudicated_metrics: dict[str, Any] = {}
    if {"metric", "raw_value", "adjudicated_value"}.issubset(fieldnames):
        csv_schema = "metric,raw_value,adjudicated_value"
        for row in rows:
            metric = str(row.get("metric", "")).strip()
            if not metric:
                continue
            raw_value = _coerce_metric_value(row.get("raw_value"))
            adjudicated_value = _coerce_metric_value(row.get("adjudicated_value"))
            if raw_value is not None:
                raw_metrics[metric] = raw_value
            if adjudicated_value is not None:
                adjudicated_metrics[metric] = adjudicated_value
            elif raw_value is not None:
                adjudicated_metrics[metric] = raw_value
    elif {"metric", "value"}.issubset(fieldnames):
        csv_schema = "metric,value"
        for row in rows:
            metric = str(row.get("metric", "")).strip()
            if not metric:
                continue
            value = _coerce_metric_value(row.get("value"))
            raw_metrics[metric] = value
            adjudicated_metrics[metric] = value
    else:
        raise ValueError(f"Unsupported sequence_metrics.csv schema: {sorted(fieldnames)}")

    return {
        "source": "sequence_metrics_csv",
        "csv_schema": csv_schema,
        "ground_truth_metrics": raw_metrics,
        "adjudicated_ground_truth_metrics": adjudicated_metrics,
    }


def load_sequence_metrics_artifact(summary_path: Path | str | None = None, metrics_csv_path: Path | str | None = None) -> dict[str, Any]:
    """Load sequence metrics with JSON-first compatibility for old and new CSV shapes."""
    summary = Path(summary_path) if summary_path is not None else None
    metrics_csv = Path(metrics_csv_path) if metrics_csv_path is not None else None
    if summary is not None and summary.exists():
        with summary.open("r", encoding="utf-8") as handle:
            summary = json.load(handle)
        return {
            "source": "sequence_validation_summary_json",
            "csv_schema": None,
            "ground_truth_metrics": summary.get("ground_truth_metrics") or {},
            "adjudicated_ground_truth_metrics": summary.get("adjudicated_ground_truth_metrics") or summary.get("ground_truth_metrics") or {},
        }
    if metrics_csv is not None and metrics_csv.exists():
        return _load_sequence_metrics_csv(metrics_csv)
    raise FileNotFoundError("No sequence metrics artifact was found.")


def compare_sequence_event_decisions(
    args: Any,
    schema: ClassSchema,
    *,
    source_mode: str = "artifact_driven",
    event_decisions_source: str = "provided_artifact",
    extra_sequence_fields: dict[str, Any] | None = None,
    extra_artifacts: dict[str, Any] | None = None,
    extra_warnings: list[dict[str, Any]] | None = None,
) -> tuple[dict[str, Any], int]:
    event_decisions_path = Path(args.event_decisions).expanduser().resolve()
    ground_truth_path = Path(args.ground_truth).expanduser().resolve()
    review_checklist_path = Path(args.review_checklist).expanduser().resolve() if getattr(args, "review_checklist", None) else None
    output_root = Path(args.output).expanduser().resolve()

    runtime_rows, runtime_warnings = _load_event_decisions(event_decisions_path, schema)
    ground_truth_payload, expected_events = _load_ground_truth(ground_truth_path)
    accepted_missed_reviews, accepted_runtime_reviews, review_rows, review_warnings = _load_review_checklist(review_checklist_path)

    sequence_dir = output_root / "sequence_validation"
    sequence_dir.mkdir(parents=True, exist_ok=True)
    matches_path = sequence_dir / "event_matches.csv"
    extras_path = sequence_dir / "unmatched_runtime_events.csv"
    metrics_path = sequence_dir / "sequence_metrics.csv"
    summary_path = sequence_dir / "sequence_validation_summary.json"

    unmatched_runtime_indexes = set(range(len(runtime_rows)))
    match_rows: list[dict[str, Any]] = []
    extra_rows: list[dict[str, Any]] = []
    matched_events = 0
    missed_events = 0
    class_matches = 0
    class_mismatches = 0
    trigger_matches = 0
    trigger_mismatches = 0
    decision_window_matches = 0
    accepted_startup_no_runtime_events = 0
    accepted_detector_false_positive_runtime_events = 0
    accepted_missed_event_ids: set[str] = set()
    accepted_runtime_event_ids: set[str] = set()

    for expected_event in expected_events:
        candidates: list[tuple[tuple[int, int, str], int]] = []
        for runtime_index in sorted(unmatched_runtime_indexes):
            basis_rank, distance, basis = _match_basis(runtime_rows[runtime_index], expected_event)
            if basis_rank < 99:
                candidates.append(((basis_rank, distance, basis), runtime_index))
        if not candidates:
            missed_events += 1
            expected_event_id = str(expected_event["event_id"])
            review = accepted_missed_reviews.get(expected_event_id)
            adjudication_status = ""
            adjudication_action = ""
            review_item_id = ""
            if review is not None:
                adjudication_status = "accepted_startup_no_runtime_coverage"
                adjudication_action = str(review.get("reviewer_action", ""))
                review_item_id = str(review.get("review_item_id", ""))
                accepted_startup_no_runtime_events += 1
                accepted_missed_event_ids.add(expected_event_id)
            match_rows.append(
                {
                    "match_status": "missed",
                    "match_basis": "no_runtime_event_in_expected_window",
                    "expected_event_id": expected_event_id,
                    "expected_event_index": expected_event["event_index"],
                    "expected_event_start": expected_event["event_window_frames"]["start"],
                    "expected_event_end": expected_event["event_window_frames"]["end"],
                    "expected_decision_start": expected_event["expected_decision_frame_window"]["start"],
                    "expected_decision_end": expected_event["expected_decision_frame_window"]["end"],
                    "expected_class_id": expected_event["expected_class_id"],
                    "expected_display_label": expected_event["expected_display_label"],
                    "expected_trigger_decision": expected_event["expected_trigger_decision"],
                    "runtime_event_id": "",
                    "runtime_first_detected_frame": "",
                    "runtime_classification_frame": "",
                    "runtime_motion_decision_frame": "",
                    "runtime_predicted_class_id": "",
                    "runtime_predicted_class_id_raw": "",
                    "runtime_predicted_label_raw": "",
                    "runtime_predicted_display_label": "",
                    "runtime_should_trigger": "",
                    "class_match": "",
                    "trigger_match": "",
                    "adjudication_status": adjudication_status,
                    "adjudication_action": adjudication_action,
                    "review_item_id": review_item_id,
                    "notes": "No unmatched runtime row overlapped the expected event window.",
                }
            )
            continue

        (_, _, basis), runtime_index = min(candidates, key=lambda item: item[0])
        unmatched_runtime_indexes.remove(runtime_index)
        runtime_row = runtime_rows[runtime_index]
        matched_events += 1
        if basis == "classification_frame_in_expected_decision_window":
            decision_window_matches += 1
        class_match = runtime_row["predicted_class_id"] == expected_event["expected_class_id"]
        trigger_match = runtime_row["should_trigger"] == bool(expected_event["expected_trigger_decision"])
        if class_match:
            class_matches += 1
        else:
            class_mismatches += 1
        if trigger_match:
            trigger_matches += 1
        else:
            trigger_mismatches += 1
        match_rows.append(
            {
                "match_status": "matched",
                "match_basis": basis,
                "expected_event_id": expected_event["event_id"],
                "expected_event_index": expected_event["event_index"],
                "expected_event_start": expected_event["event_window_frames"]["start"],
                "expected_event_end": expected_event["event_window_frames"]["end"],
                "expected_decision_start": expected_event["expected_decision_frame_window"]["start"],
                "expected_decision_end": expected_event["expected_decision_frame_window"]["end"],
                "expected_class_id": expected_event["expected_class_id"],
                "expected_display_label": expected_event["expected_display_label"],
                "expected_trigger_decision": expected_event["expected_trigger_decision"],
                "runtime_event_id": runtime_row["runtime_event_id"],
                "runtime_first_detected_frame": runtime_row["first_detected_frame"],
                "runtime_classification_frame": runtime_row["classification_frame"],
                "runtime_motion_decision_frame": runtime_row["motion_decision_frame"],
                "runtime_predicted_class_id": runtime_row["predicted_class_id"],
                "runtime_predicted_class_id_raw": runtime_row["predicted_class_id_raw"],
                "runtime_predicted_label_raw": runtime_row["predicted_label_raw"],
                "runtime_predicted_display_label": runtime_row["predicted_display_label"],
                "runtime_should_trigger": runtime_row["should_trigger"],
                "class_match": class_match,
                "trigger_match": trigger_match,
                "adjudication_status": "",
                "adjudication_action": "",
                "review_item_id": "",
                "notes": "",
            }
        )

    for runtime_index in sorted(unmatched_runtime_indexes):
        runtime_row = runtime_rows[runtime_index]
        nearest_expected_id, nearest_distance = _nearest_expected(runtime_row, expected_events)
        runtime_event_id = str(runtime_row["runtime_event_id"])
        review = accepted_runtime_reviews.get(runtime_event_id)
        adjudication_status = ""
        adjudication_action = ""
        review_item_id = ""
        if review is not None:
            adjudication_status = "accepted_detector_false_positive"
            adjudication_action = str(review.get("reviewer_action", ""))
            review_item_id = str(review.get("review_item_id", ""))
            accepted_detector_false_positive_runtime_events += 1
            accepted_runtime_event_ids.add(runtime_event_id)
        extra_rows.append(
            {
                "runtime_event_id": runtime_event_id,
                "source_sequence_id": runtime_row["source_sequence_id"],
                "runtime_first_detected_frame": runtime_row["first_detected_frame"],
                "runtime_classification_frame": runtime_row["classification_frame"],
                "runtime_motion_decision_frame": runtime_row["motion_decision_frame"],
                "runtime_predicted_class_id": runtime_row["predicted_class_id"],
                "runtime_predicted_class_id_raw": runtime_row["predicted_class_id_raw"],
                "runtime_predicted_label_raw": runtime_row["predicted_label_raw"],
                "runtime_should_trigger": runtime_row["should_trigger"],
                "nearest_expected_event_id": nearest_expected_id or "",
                "nearest_expected_center_distance_frames": nearest_distance if nearest_distance is not None else "",
                "adjudication_status": adjudication_status,
                "adjudication_action": adjudication_action,
                "review_item_id": review_item_id,
            }
        )

    warnings = list(runtime_warnings)
    warnings.extend(review_warnings)
    warnings.extend(extra_warnings or [])
    warnings.append(
        {
            "code": "PROVISIONAL_INTERNAL_GROUND_TRUTH",
            "message": "Sequence ground truth is internal and provisional; do not use these results for public or final sequence accuracy claims.",
            "details": {"ground_truth_status": ground_truth_payload.get("status"), "manual_review_status": ground_truth_payload.get("provenance", {}).get("manual_review_status")},
        }
    )
    if any(row["normalization_status"] == "alias" for row in runtime_rows):
        warnings.append(
            {
                "code": "RUNTIME_LABELS_NORMALIZED_VIA_ALIASES",
                "message": "Runtime event labels were normalized through schema aliases before comparison.",
                "details": {
                    "binary_mapping": {
                        "0": ["Empty", "MoreThanTwo", "Waste"],
                        "1": ["Single", "Hits"],
                    }
                },
            }
        )
    if "wave04_runtime_replay" in event_decisions_path.as_posix():
        warnings.append(
            {
                "code": "BINARY_REPLAY_ARTIFACT_NOT_AVAILABLE",
                "message": "This validation run used the accepted Wave 04 legacy replay artifact because the Wave 05 binary runtime replay event-decision CSV is not present in the workspace.",
                "details": {"event_decisions_path": str(event_decisions_path)},
            }
        )

    ground_truth_metrics = {
        "expected_events_total": len(expected_events),
        "runtime_events_total": len(runtime_rows),
        "matched_events": matched_events,
        "missed_events": missed_events,
        "extra_runtime_events": len(extra_rows),
        "class_matches": class_matches,
        "class_mismatches": class_mismatches,
        "trigger_matches": trigger_matches,
        "trigger_mismatches": trigger_mismatches,
        "decision_window_matches": decision_window_matches,
        "class_accuracy_on_matched": (class_matches / matched_events) if matched_events else None,
        "trigger_accuracy_on_matched": (trigger_matches / matched_events) if matched_events else None,
    }
    adjudicated_ground_truth_metrics = {
        "expected_events_total": len(expected_events),
        "runtime_events_total": len(runtime_rows),
        "matched_events": matched_events,
        "missed_events": missed_events - accepted_startup_no_runtime_events,
        "extra_runtime_events": len(extra_rows) - accepted_detector_false_positive_runtime_events,
        "class_matches": class_matches,
        "class_mismatches": class_mismatches,
        "trigger_matches": trigger_matches,
        "trigger_mismatches": trigger_mismatches,
        "decision_window_matches": decision_window_matches,
        "class_accuracy_on_matched": (class_matches / matched_events) if matched_events else None,
        "trigger_accuracy_on_matched": (trigger_matches / matched_events) if matched_events else None,
        "accepted_detector_false_positive_runtime_events": accepted_detector_false_positive_runtime_events,
        "accepted_startup_no_runtime_events": accepted_startup_no_runtime_events,
        "unresolved_missed_events": missed_events - accepted_startup_no_runtime_events,
        "unresolved_extra_runtime_events": len(extra_rows) - accepted_detector_false_positive_runtime_events,
    }
    unmatched_review_expected_ids = sorted(set(accepted_missed_reviews) - accepted_missed_event_ids)
    unmatched_review_runtime_ids = sorted(set(accepted_runtime_reviews) - accepted_runtime_event_ids)
    if unmatched_review_expected_ids:
        warnings.append(
            {
                "code": "REVIEW_CHECKLIST_ACCEPTED_MISSED_NOT_IN_RAW_MISSES",
                "message": "Some accepted missed-event review rows did not correspond to a raw missed expected event.",
                "details": {"expected_event_ids": unmatched_review_expected_ids},
            }
        )
    if unmatched_review_runtime_ids:
        warnings.append(
            {
                "code": "REVIEW_CHECKLIST_ACCEPTED_RUNTIME_NOT_IN_RAW_EXTRAS",
                "message": "Some accepted detector-false-positive review rows did not correspond to raw unmatched runtime events.",
                "details": {"runtime_event_ids": unmatched_review_runtime_ids},
            }
        )

    summary = {
        "schema_version": "validator.v1",
        "run_id": output_root.name,
        "mode": "sequence",
        "status": "completed",
        "created_at": utc_now(),
        "internal_provisional": True,
        "sequence": {
            "event_decisions_path": str(event_decisions_path),
            "source_mode": source_mode,
            "event_decisions_source": event_decisions_source,
            "ground_truth_path": str(ground_truth_path),
            "review_checklist_path": str(review_checklist_path) if review_checklist_path else None,
            "source_sequence_id": runtime_rows[0]["source_sequence_id"] if runtime_rows else None,
            "ground_truth_fixture_id": ground_truth_payload.get("fixture_id"),
            **(extra_sequence_fields or {}),
        },
        "class_schema": {
            "label_schema_version": schema.schema_id,
            "classes": schema.classes,
            "display_labels": schema.display_labels,
        },
        "ground_truth_metrics": ground_truth_metrics,
        "adjudicated_ground_truth_metrics": adjudicated_ground_truth_metrics,
        "review_adjudication": {
            "review_checklist_path": str(review_checklist_path) if review_checklist_path else None,
            "review_rows_total": len(review_rows),
            "accepted_detector_false_positive_runtime_events": accepted_detector_false_positive_runtime_events,
            "accepted_startup_no_runtime_events": accepted_startup_no_runtime_events,
            "unmatched_accepted_missed_event_ids": unmatched_review_expected_ids,
            "unmatched_accepted_runtime_event_ids": unmatched_review_runtime_ids,
        },
        "artifacts": {
            "event_matches_csv": str(matches_path),
            "unmatched_runtime_events_csv": str(extras_path),
            "sequence_metrics_csv": str(metrics_path),
            **(extra_artifacts or {}),
        },
        "warnings": warnings,
        "errors": [],
    }

    write_csv(
        matches_path,
        match_rows,
        [
            "match_status",
            "match_basis",
            "expected_event_id",
            "expected_event_index",
            "expected_event_start",
            "expected_event_end",
            "expected_decision_start",
            "expected_decision_end",
            "expected_class_id",
            "expected_display_label",
            "expected_trigger_decision",
            "runtime_event_id",
            "runtime_first_detected_frame",
            "runtime_classification_frame",
            "runtime_motion_decision_frame",
            "runtime_predicted_class_id",
            "runtime_predicted_class_id_raw",
            "runtime_predicted_label_raw",
            "runtime_predicted_display_label",
            "runtime_should_trigger",
            "class_match",
            "trigger_match",
            "adjudication_status",
            "adjudication_action",
            "review_item_id",
            "notes",
        ],
    )
    write_csv(
        extras_path,
        extra_rows,
        [
            "runtime_event_id",
            "source_sequence_id",
            "runtime_first_detected_frame",
            "runtime_classification_frame",
            "runtime_motion_decision_frame",
            "runtime_predicted_class_id",
            "runtime_predicted_class_id_raw",
            "runtime_predicted_label_raw",
            "runtime_should_trigger",
            "nearest_expected_event_id",
            "nearest_expected_center_distance_frames",
            "adjudication_status",
            "adjudication_action",
            "review_item_id",
        ],
    )
    write_csv(metrics_path, _flatten_metrics(summary), ["metric", "raw_value", "adjudicated_value"])
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    payload = {
        "schema_version": 1,
        "command": "validate-sequence",
        "status": "ok",
        "timestamp": utc_now(),
        "message": "Sequence event-decision comparison completed.",
        "summary_path": str(summary_path),
        "ground_truth_metrics": ground_truth_metrics,
        "adjudicated_ground_truth_metrics": adjudicated_ground_truth_metrics,
        "warnings": warnings,
        "errors": [],
    }
    return payload, 0 if not runtime_warnings else EXIT_SCHEMA_MISMATCH


def run_validate_sequence(args: Any, schema: ClassSchema) -> tuple[dict[str, Any], int]:
    return compare_sequence_event_decisions(args, schema)
