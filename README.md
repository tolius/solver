# Solver

Solver is a graphical user interface for solving [antichess](https://en.wikipedia.org/wiki/Antichess) openings. Priority is placed on minimising the winning length over reducing the number of nodes. Some results are available [here](https://antichess.onrender.com/). The program was originally written in Python. This version is a conversion of existing code into C++.

How to use the program is described [here](https://github.com/tolius/solver/wiki).

The following libraries/modules/components are used in the program:
- [Qt 5.15.2](https://download.qt.io/archive/qt/5.15/5.15.2/)
- [Cute Chess 1.3.1](https://github.com/cutechess/cutechess/releases/tag/v1.3.1) (modified)
- [Fairy-Stockfish](https://github.com/fairy-stockfish/Fairy-Stockfish/tree/104d2f40e4d064815d6b06d0c812aec3b7b01f20) (modified)
- [Multi-Variant Stockfish](https://github.com/ddugovic/Stockfish/tree/146269195b1b6a5e9d1121d9fd5767668a48a2a6) (modified)
- [Library to query Watkins proof tables](https://github.com/niklasf/antichess-tree-server/tree/097dbbce7253151813b06d2e1ad0861ac4e5864f) (modified)
- QSS templates of [DevSec Studio](https://qss-stock.devsecstudio.com/templates.php) (modified)
- QSS templates of [GTRONICK](https://github.com/GTRONICK/QSS/blob/master/MaterialDark.qss) (modified)
- Dictzip from [dictd 1.12.1](https://sourceforge.net/projects/dict/) with [Tvangeste's tweaks](https://github.com/Tvangeste/dictzip-win32/tree/bb996c999e9f437b1abb98d941a0a7a98ba82f67) (modified)
- [zlib 1.3.0.1](https://github.com/madler/zlib/tree/643e17b7498d12ab8d15565662880579692f769d)

## Installation

### Binaries

See the [Releases](https://github.com/tolius/solver/releases) page.

### Building from source

Solver requires Qt 5.15 or greater, a compiler with C++17 support, and CMake.
[Fairy-Stockfish](https://github.com/tolius/fairy-stockfish-egtb) with support of DTW antichess endgame tablebases is supposed to be used as an engine.

### Endgame tablebase files

Endgame tablebase files are available on request. The 2-4-piece tablebase consists of 714 files of 500MB in size. The efficiency of the program is greatly reduced if you do not use at least 4-piece tablebase with DTW (Depth To Win) metric. Other metrics are not supported.

## License

Solver is released under the GPLv3+ license except for the components in the `projects/lib/components`, `projects/gui/components`, and `projects/gui/res/styles` directories which are released under the MIT License.

## Credits

Solver is written by Tolius.
