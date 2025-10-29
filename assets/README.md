Nothofagus is not meant to deal with assets, the goal is a single static library simplifying rendering and user triggered events, not asset management.
However, in order to scale DearImgui properly on modern displays, an external font file is required to build the font atlas at the desired resolution.
We are using Roboto, downloaded from Google Fonts: https://fonts.google.com/specimen/Roboto
The original source file is added to the repository for reference, however, we convert it to a binary data and stored in a header file for its usage in the code base.

To rebuild the header file `roboto_font.h` with the binary data, you need the unix tool `xxd`, and run it from the root of this repo as follow:
```
xxd -i assets/Roboto-VariableFont_wdth,wght.ttf source/roboto_font.h
```