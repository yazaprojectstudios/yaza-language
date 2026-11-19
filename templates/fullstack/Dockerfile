# Multi-stage YAZA Fullstack Monolith container (Single native binary + static public assets)
FROM debian:bookworm-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libargon2-1 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY {{project_name}} /app/{{project_name}}
COPY yaza.toml /app/yaza.toml
COPY public /app/public
RUN mkdir -p /app/data && chmod 700 /app/data
EXPOSE 8080
ENV YAZA_ENV=production
CMD ["/app/{{project_name}}"]
