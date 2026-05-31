# BX Crawl

A small Moldvay Basic–inspired dungeon crawler prototype written in C++.

The initial goal is **very** simple:

- One hero (`@`)
- One goblin (`g`)
- Tiny ASCII room
- Bump to attack
- BX-flavored stats and combat

Over time this will grow into a more complete B/X-style dungeon romp.

## Building

Requires:

- CMake 3.10+
- A C++17-capable compiler (g++, clang++, MSVC)

```bash
git clone <your-repo-url> bx-crawl
cd bx-crawl
mkdir build
cd build
cmake ..
cmake --build .
./bx_crawl          # Linux/macOS
# or
bx_crawl.exe        # Windows

## Controls
- w, a, s, d – move
- Bump into the goblin to attack
- q – quit

See [Monsters.md](Monsters.md) for creature types and [Scrolls.md](Scrolls.md) for scroll spells.
