Nothofagus is not meant to deal with assets; the goal is a single static library simplifying rendering and user-triggered events, not asset management.
However, in order to scale DearImgui properly on modern displays — and to render markdown with real bold/italic/bold-italic/monospace faces — embedded font files are required to build the font atlas at the desired resolution.

We embed the **Noto Sans** family (Regular, Bold, Italic, BoldItalic) plus **Noto Sans Mono** for code, downloaded from the official Noto Fonts project (https://github.com/notofonts) / Google Fonts (https://fonts.google.com/noto). They are licensed under the SIL Open Font License 1.1 — see `OFL.txt`.

Optional **CJK** faces — `NotoSansSC-Regular.ttf` (Simplified Chinese), `NotoSansTC-Regular.ttf` (Traditional Chinese), `NotoSansJP-Regular.ttf` (Japanese), `NotoSansKR-Regular.ttf` (Korean) — are also Noto Sans / OFL 1.1. They are **static Regular (wght=400) instances** of the upstream Google Fonts variable fonts (`NotoSans{SC,TC,JP,KR}[wght].ttf`), TrueType-outline (`glyf`) so `stb_truetype` rasterizes them reliably. They are embedded **only** when the matching `-DNOTHOFAGUS_EMBED_CJK_*` CMake option is ON (all default OFF); a default build pays nothing for them.

The `.ttf` files in this folder are the reference sources. The compressed C byte arrays are generated **automatically at build time** by CMake — no manual step. It works under clang-cl on Windows.

- The vendored `third_party/imgui/misc/fonts/binary_to_compressed_c.cpp` is built as a host tool (`nothofagus_font_compressor`) that `stb_compress`es each `.ttf`. `cmake/run_font_compressor.cmake` runs it and writes a `generated/noto_*.cpp` translation unit defining `<symbol>_compressed_data[]` / `<symbol>_compressed_size`.
- The matching `extern` declarations live in a generated `generated/embedded_fonts.h`.
- At runtime ImGui decompresses via `AddFontFromMemoryCompressedTTF` (its `stb_decompress` is built in) — no external runtime dependency. Compression shrinks the embedded Latin family ~35% (3.78 MB → 2.46 MB).
- Generation re-runs only when a `.ttf`, the compressor tool, or the wrapper script changes (`DEPENDS` edges) — never on a bare reconfigure.

Generated output lands in `build/<preset>/generated/` and is gitignored. To swap a face, drop a replacement `.ttf` in this folder (keeping the same filename) and rebuild — the array regenerates on its own.
