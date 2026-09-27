# Score input recording verification

Scope: upload-only recording after the separate upstream merge `db148d5f`.
No replay fetch, polling, download or playback is present in `src/`.

Passed locally:

- Host C++ client against `tests/fanmade/fixture.py`: simultaneous hits, negative
  and decreasing game timestamps, audio/visual delay snapshots, unchanged durable
  request after a simulated commit followed by HTTP 500 and process restart,
  legacy server payloads without the additional field, and no replay requests.
  Existing guest/auth, proxy, catalog, download, cache and score checks also pass.
- iOS Simulator Release: `cmake --build build-ios-simulator --config Release
  --parallel 4` completed successfully. No device installation or user data was
  replaced. This is build verification, not physical-device gameplay validation.
- Separate merge checks are recorded in `../upstream/MERGE_2026-09-26.md`.

The corresponding Fanmade backend passes its full PostgreSQL/HTTP suite with
`go test -race ./...` and `go vet ./...`, including omitted/null/malformed recording
attachments, preservation of old digests, migration from pre-021 scores, large
recordings and changed-recording idempotency conflicts.
