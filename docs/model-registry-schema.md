# Model Registry Schema

This document describes the runtime model registry shape currently consumed by the desktop app's `model_registry_service`.

## Top-level object

Required keys:

- `schema_version`: currently `model-registry-v1`
- `entries`: array of registry entry objects

Observed companion keys used by the packaged registry or fallback registry:

- `registry_id`
- `source`

If `schema_version` is missing, unsupported, or `entries` is not an array, the desktop app falls back to the temporary static registry embedded in `model_registry_service.cpp`.

## Entry object

Fields currently consumed by the desktop app:

- `registry_entry_id`
- `display_name`
- `state`
- `live_use_mode`
- `selectable_for_normal_live_sorting`
- `model_path`
- `model_sha256`
- `metadata_path`
- `metadata_sha256`
- `metadata_status`
- `validation_status`
- `promotion_status`
- `promotion_record_path`
- `classes`
- `display_labels`
- `target_policy`
- `limitations`
- `blockers`

Additional fields may exist in registry files. The current service does not reject extra keys.

## `target_policy`

The desktop app currently reads:

- `target_class_id`
- `target_display_label`

The packaged fallback also carries:

- `waste_class_id`

## `display_labels`

`display_labels` is expected to be an object keyed by class id. The current summary and UI logic use those values as display text and fall back to the class id if a label is missing.

## `blockers`

`blockers` is read as an array. Summary formatting looks for blocker objects with:

- `blocker`
- `required_next_action`

If either field is absent, the current summary logic simply omits the missing text.

## Fallback behavior

The desktop app preserves the existing fallback rules:

1. Resolve the registry path with `modelRegistryPath()`.
2. Try to open and parse the file.
3. Require `schema_version == "model-registry-v1"` and an `entries` array.
4. On any read, parse, or schema failure, use the embedded temporary static registry.
5. If the loaded registry has zero entries, `main.cpp` still swaps in the same temporary static registry and reports the existing warning text.

## Path resolution behavior

Registry paths are still interpreted exactly as before:

- Absolute paths are used directly.
- Relative paths are resolved against the project root when available.
- Runtime model artifacts also probe `internal-release/` and packaged `app/runtime/models/` locations.
- Default workspace asset staging still copies the active packaged model and metadata into the user's `Documents/OpenDSS/models` directory and copies the prepared dataset when present.
