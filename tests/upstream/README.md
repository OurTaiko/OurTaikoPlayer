# Curated upstream regression checks

Run with Clang on macOS or Linux after fetching the CMake dependencies and skin
submodules:

```sh
python3 tests/upstream/run.py
# For an iOS dependency cache or an external skin checkout:
python3 tests/upstream/run.py --deps build-ios-simulator/_deps --skins /path/to/Skins
```

The runner generates headers in a temporary directory and enables AddressSanitizer
and UndefinedBehaviorSanitizer. It covers:

- Real exam parsing and judging: course-wide and per-song borders, legacy flat
  `gothrough=false`, omitted gold, perfect borders equal to red, invalid gold,
  malformed nested pairs, and all 25 bundled courses. Nested pairs always select
  per-song judging. Invalid nested entries reject the course without shifting
  later borders. An incomplete per-song course is not offered for play.
- Existing selection rules for all 32 combinations of ordinary difficulties,
  preserving the main branch's standalone Edit support.
- The real font cache with a fake rasterizer: two missing characters sharing one
  fallback are rasterized once, own separate images, and unload without leaks or
  duplicate frees. This checks ownership and caching, not visual typography.
- The real texture resolver with in-memory texture objects: selected language,
  Japanese/English fallback among loaded textures, skin unload, and substitution
  only for the drumroll icon. No GPU is required.
- The real touch callback (shared and iOS code paths): release/cancel during an
  input lock, the first hit after reusing a finger ID, ignored locked presses,
  and independent cleanup of multiple fingers. Screen dimensions and the lock
  state are supplied by the fixture; no SDL window is required.

Manual gameplay checks still needed: per-song course progression and gold verdict,
Back/Restart during a song's tail, skipped-run result notice and unchanged records,
classic and Nijiiro HUD layouts, and option popups in 1P/2P. Test Japanese, English,
and Chinese fonts/textures. The headless checks do not establish device rendering
or Windows/Android runtime behavior.

On a touch device, tap immediately before the song transition and lift during
it, then verify the first gameplay hit registers. Repeat with multiple fingers
and after canceling a touch. Also verify transition-time presses cannot trigger
practice controls. The callback checks do not replace these device checks.
