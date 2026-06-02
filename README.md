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

- CMake 3.20+
- A C++20-capable compiler (g++, clang++, MSVC)
- A POSIX-like environment for the text frontend

```bash
git clone <your-repo-url> bx-crawl
cd bx-crawl
cmake -S . -B build
cmake --build build
./build/frontends/text/bx_crawl_text
```

## Controls
- w, a, s, d – move
- Bump into the goblin to attack
- q – quit

See [Monsters.md](Monsters.md) for creature types and [Scrolls.md](Scrolls.md) for scroll spells.
