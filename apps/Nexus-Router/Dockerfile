FROM oven/bun:1 as builder
WORKDIR /build
COPY package.json bun.lockb* ./
RUN bun install --production --frozen-lockfile 2>/dev/null || bun install --production
COPY src ./src
COPY tsconfig.json biome.json ./
RUN bun build src/index.ts --outdir dist --target bun

FROM oven/bun:1
WORKDIR /app
COPY --from=builder /build/dist ./dist
COPY --from=builder /build/node_modules ./node_modules
COPY config ./config
EXPOSE 9999
HEALTHCHECK --interval=15s --timeout=5s --retries=3 --start-period=10s \
  CMD bun -e "fetch('http://localhost:9999/health').then(r=>{if(!r.ok)process.exit(1)})"
CMD ["bun", "run", "dist/index.js"]
