Nothofagus is not meant to deal with assets; the goal is a single static library simplifying rendering and user-triggered events, not asset management.
However, in order to scale DearImgui properly on modern displays — and to render markdown with real bold/italic/bold-italic/monospace faces — embedded font files are required to build the font atlas at the desired resolution.

We embed the **Noto Sans** family (Regular, Bold, Italic, BoldItalic) plus **Noto Sans Mono** for code, downloaded from the official Noto Fonts project (https://github.com/notofonts) / Google Fonts (https://fonts.google.com/noto). They are licensed under the SIL Open Font License 1.1 — see `OFL.txt`.

The five `.ttf` static instances in this folder are the reference sources. The C byte arrays are generated **automatically at build time** by CMake — no manual step. It works under clang-cl on Windows.

- `cmake/embed_font.cmake` reads each `.ttf` via `file(READ ... HEX)` and writes a `generated/noto_sans_*.cpp` translation unit defining `notoSans<Face>Ttf[]` / `notoSans<Face>TtfLen`.
- The matching `extern` declarations live in a generated `generated/embedded_fonts.h`.
- Generation re-runs only when a `.ttf` (or the script) changes, via the `DEPENDS` edge.

Generated output lands in `build/<preset>/generated/` and is gitignored. To swap a face, drop a replacement `.ttf` in this folder (keeping the same filename) and rebuild — the array regenerates on its own.
