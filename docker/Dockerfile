FROM gcc:13-bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY scripts/ scripts/
COPY data/ data/
COPY modules/ modules/
COPY include/ include/
COPY CMakeLists.txt .
COPY Monsters.md Scrolls.md ./
COPY engine/ engine/
COPY frontends/ frontends/
COPY tests/ tests/
COPY tools/ tools/

RUN mkdir -p build && \
    cmake -S . -B build && \
    make -C build -j$(nproc)

FROM gcc:13-bookworm AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /game

COPY --from=builder /src/build/frontends/text/bx_crawl_text .
COPY --from=builder /src/data /data
COPY --from=builder /src/modules /modules
COPY --from=builder /src/Monsters.md .
COPY --from=builder /src/Scrolls.md .

RUN echo '#!/bin/bash' > /game/run.sh && \
    echo 'cd /game && ./bx_crawl_text' >> /game/run.sh && \
    chmod +x /game/run.sh

CMD ["/game/run.sh"]
