# Runtime Logs

OpenDSS runtime log writing is isolated in the desktop app writer modules:

- `app/runtime/desktop_app/live_log_writer.*` writes live pipeline CSV logs and the live-derived sequence log.
- `app/runtime/desktop_app/sequence_summary_writer.*` writes sequence replay CSV rows, event trajectories, and motion-alignment summaries.

The writers preserve the existing runtime filenames and CSV formats:

- `live_<timestamp>_live_log.csv`
- `sequence_test_log_live_<timestamp>.csv`
- `sequence_event_trajectory_live_<timestamp>.csv`
- `sequence_summary_live_<timestamp>.csv`
- `sequence_test_log_<timestamp>.csv`
- `sequence_event_trajectory_<timestamp>.csv`
- `sequence_summary_<timestamp>.csv`

`MainWindow` still owns record construction from camera, pipeline, GUI, and DAQ state. The sequence replay thread body also remains in `MainWindow`; it now passes value rows to `SequenceLogWriter` so CSV headers, quoting, numeric formatting, and flushing are handled by the writer module.
