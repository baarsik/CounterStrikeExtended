# Counter-Strike death notice alignment fixture

The extractor compiles the complete production `CHudDeathNotice::Draw` and
`GetDeathNoticeY` bodies, the notice structure/list/display duration, and the
actual console-string drawing/measurement wrappers. It records source SHA-256
hashes beside the generated includes. Controlled external seams supply console
text metrics, sprite rectangles, viewport state and draw destinations. Expected
coordinates are literal, hand-checked values; the fixture contains no duplicate
of the production alignment calculation.

```sh
cmake -S tests/native/death_notice -B build/death-notice-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/death-notice-debug --parallel 2
ctest --test-dir build/death-notice-debug --output-on-failure
```

Use a separate Release directory and add
`-DCSE_DEATH_NOTICE_SANITIZERS=ON` for Clang/GCC ASan+UBSan. MSVC uses `/W4 /WX`.

The sixteen cases cover unchanged sprite Y at text heights 13/16, centering at
21/28/33/70, independent weapon/headshot heights, a sprite taller than the text,
no headshot, suicide, non-player victims, skull fallback, multiple row spacing,
horizontal positions/order/colors, expiry/compaction/deadline capping, tutor and
print gating, and both spectator top-bar paths. Removing either sprite's Y
adjustment fails the recorded draw-destination assertions.

This fixture checks production layout behavior with supplied metrics. It does
not initialize a font provider, open a window, run the game or establish visual
acceptance of any raster font. Owning client compilation and runtime acceptance
remain separate evidence.
