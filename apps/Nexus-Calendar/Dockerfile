# Build from the REPOSITORY ROOT, not this folder:
#   docker build -f apps/Nexus-Calendar/Dockerfile .
# Calendar imports packages/nexus-identity, which lives outside the app, so an
# app-scoped context cannot contain it. Dockerfile.dockerignore (next to this
# file) narrows the root context to exactly the paths copied below.
FROM oven/bun:1.3.12-alpine

WORKDIR /app

COPY --chown=bun:bun packages/nexus-identity/package.json packages/nexus-identity/package.json
COPY --chown=bun:bun packages/nexus-identity/src packages/nexus-identity/src
COPY --chown=bun:bun apps/Nexus-Calendar/package.json apps/Nexus-Calendar/package.json
COPY --chown=bun:bun apps/Nexus-Calendar/src apps/Nexus-Calendar/src

RUN mkdir -p /app/data && chown -R bun:bun /app/data

ENV PORT=3068 \
    NEXUS_BIND_HOST=0.0.0.0 \
    NEXUS_CALENDAR_DB=/app/data/calendar.sqlite

WORKDIR /app/apps/Nexus-Calendar
EXPOSE 3068
USER bun

CMD ["bun", "run", "src/index.ts"]
