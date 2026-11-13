# extras

Tools and license files that support the library but are not compiled into it.

- **`fontgen/`** — `make_terminal_fonts.py`, which builds the text terminal's fonts
  (`src/p486t_font_mono16.cpp` and the opt-in `p486t_font_mono16_*.h` headers) from
  Inconsolata, DejaVu Sans Mono, Font Awesome Free and Noto Sans CJK, together with
  those fonts' license texts. The fonts in `src/` are rendered from them and stay under
  their licenses. The script and fonts come from `chipguy_ESP32-CYD-Resistive_display`,
  with the output names changed to this library's `p486t_` prefix; that library's
  Chinese, Japanese-kanji and Korean fonts can be rebuilt for this one with it.
- **`bitmaps/`** — `make_header_footer.py`, which renders the TerminalHeaderFooter
  example's header and footer images (`examples/TerminalHeaderFooter/bitmaps.h`). Needs
  Pillow and the DejaVu Sans font files.
