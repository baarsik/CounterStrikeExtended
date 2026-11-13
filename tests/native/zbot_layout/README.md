# ZBot control layout

`radio_image_tests.py` compiles the actual shared and CS `RadioImage` owners with
controlled bitmap/scale inputs under ASan/UBSan. It reproduces late parenting at
1x, 2x and 3x: marker bounds must reserve the rendered icon plus a text gap, paint
at the position assigned by Label, and restore the shared bitmap dimensions.

`layout_tests.cpp` uses the real shared VGUI controls, resource parser, schemes
and fonts to apply both bundled ZBot page layouts. It checks translated label
content in the dialog's 436×365 page viewport at 2x HD scale and captures both pages.
Cvar checkboxes
use their unchanged `CheckButton` geometry; no game or engine cvars are changed.

Build with `tests/native/server_browser_ui/build_vgui_component.py --driver
zbot_layout`, supplying a current owning `--recipe-cache` and `--sdl-library`.
Run the resulting `vgui_tests` with:

```text
vgui-image filesystem-image generated-translations zbot-cstrike platform valve output language
```

Use `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software`.
The generated packet resources supply translations; the platform/valve inputs
are read-only game resources. Results are `fit.tsv`, `ServerPage.bmp` and
`BotPage.bmp`. The original Russian layout fails on clipped captions and
behavior checkboxes. This fixture does not launch the game.

`run_layout.py --component <built-packet> --game <Half-Life-root>` runs both pages
in all ten bundled languages and writes per-language fit reports and screenshots.
The standalone fixture sets page bounds and HD scaling explicitly; it does not
instantiate the complete game dialog or qualify in-game input behavior.
