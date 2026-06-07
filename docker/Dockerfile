FROM gcc:13-bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY scripts/ scripts/
COPY dnd-data/schemas/ dnd-data/schemas/
COPY dnd-data/data/ dnd-data/data/
COPY include/ include/
COPY CMakeLists.txt .
COPY engine/ engine/
COPY frontends/ frontends/
COPY tests/ tests/

RUN python3 scripts/gen_data.py && \
    mkdir -p build && \
    cmake -S . -B build && \
    make -C build -j$(nproc)

FROM gcc:13-bookworm AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /game

COPY --from=builder /src/build/frontends/text/bx_crawl .
COPY --from=builder /src/Monsters.md .
COPY --from=builder /src/Scrolls.md .

RUN echo '#!/bin/bash' > /game/run.sh && \
    echo 'cd /game && ./bx_crawl' >> /game/run.sh && \
    chmod +x /game/run.sh

CMD ["/game/run.sh"]
