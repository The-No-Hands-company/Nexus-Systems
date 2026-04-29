FROM oven/bun:1.1.0 AS builder
WORKDIR /app
COPY package.json bun.lockb ./
RUN bun install --frozen-lockfile
COPY tsconfig.json ./
COPY src ./src
RUN bun run build

FROM oven/bun:1.1.0
WORKDIR /app
RUN apt-get update && apt-get install -y git git-lfs openssh-client && rm -rf /var/lib/apt/lists/*
COPY package.json ./
RUN bun install --frozen-lockfile --production
COPY --from=builder /app/dist ./dist

ENV FORGE_STORAGE_PATH=/data/forge-repos
ENV FORGE_DB_PATH=/data/forge-meta.db
ENV FORGE_PUBLIC_DOMAIN=localhost:8090
ENV PORT=8090

VOLUME ["/data"]
EXPOSE 8090 8091

CMD ["bun", "dist/backend/main.js"]
