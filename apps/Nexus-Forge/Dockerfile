# The backend has no npm runtime dependencies (bun:sqlite is built in), and
# it runs from source: the git hooks are .ts files that git executes from
# src/backend/hooks/, which a bundled dist/ would not contain.
FROM oven/bun:1.3.12
WORKDIR /app
RUN apt-get update \
  && apt-get install -y --no-install-recommends git openssh-client \
  && rm -rf /var/lib/apt/lists/*
COPY src/backend ./src/backend
COPY src/cli ./src/cli

ENV FORGE_STORAGE_PATH=/data/repos
ENV FORGE_DB_PATH=/data/forge.db
# Inside the container the server must listen on all interfaces; publish the
# port on the host's loopback only (see docker-compose.yml).
ENV HOST=0.0.0.0
ENV PORT=8094

VOLUME ["/data"]
EXPOSE 8094

CMD ["bun", "src/backend/main.ts"]
