![Scrap splash](/extras/scrap_splash.png)

# Scrap

Scrap is a new block based programming language with the aim towards advanced users. 
It is written in pure C and mostly inspired by other block based languages such as [Scratch](https://scratch.mit.edu/) and
its forks such as [Turbowarp](https://turbowarp.org).

This repository is a version of Scrap that runs in a web browser using Emscripten. Try it here: https://davidtheplatform.github.io/scrap-web/build/scrap.html

There are still some issues, namely saving/loading does not work.

## ⚠️ WARNING ⚠️

Scrap is currently in **Beta** stage. Some features may be missing or break, so use with caution!

## Controls

- Click on blocks to pick up them, click again to drop them
- You can use `Ctrl` to take only one block and `Alt` to pick up its duplicate
- Hold left mouse button to move around code space
- Holding middle mouse button will do the same, except it works all the time
- Press `Space` to jump to chain in code base (Useful if you got lost in code base)

## Screenshots

![Screenshot1](/extras/scrap_screenshot1.png)
![Screenshot2](/extras/scrap_screenshot2.png)
![Screenshot3](/extras/scrap_screenshot3.png)

## Building

### Dependencies

Before building you need to have [Raylib](https://github.com/raysan5/raylib) built and installed on your system 
**(Make sure you use Raylib 5.0 and enabled SUPPORT_FILEFORMAT_SVG in `config.h` or else it will not build properly!)**.

Make sure to build raylib using its HTML5/emscripten instructions.

### Build

This repository only supports web builds with emscripten.

Run `make RAYLIB_DIR=<path to raylib>`. The built files will be placed in `build/`. Run `python3 server.py` and navigate to `localhost:8000/build/scrap.html` to view them. 

## Wait, there is more?

In `examples/` folder you can find some example code writen in Scrap that uses most features from Scrap

In `extras/` folder you can find some various artwork made for Scrap. 
The splash art was made by [@FlaffyTheBest](https://scratch.mit.edu/users/FlaffyTheBest/), 
the logo was made by [@Grisshink](https://github.com/Grisshink) with some inspiration for logo from [@unixource](https://github.com/unixource), 
the wallpaper was made by [@Grisshink](https://github.com/Grisshink)

## License

All scrap code is licensed under the terms of [GPLv3 license](/LICENSE).
