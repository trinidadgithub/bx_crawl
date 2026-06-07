#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -t bx-crawl:latest -f docker/Dockerfile .
echo "Build complete. Run with: docker compose -f docker/docker-compose.yml up"
