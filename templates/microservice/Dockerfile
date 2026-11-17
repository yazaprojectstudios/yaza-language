# Multi-stage minimal container build for YAZA Microservice: {{project_name}}
# Produces an ultra-lightweight, zero-GC, single static native binary.

FROM debian:13-slim AS builder
WORKDIR /workspace

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang \
    llvm \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . .

# Compile optimized native binary with YAZA LLVM
RUN yaza build --release src/main.yz -o build/service

FROM gcr.io/distroless/cc-debian12
WORKDIR /app

COPY --from=builder /workspace/build/service /app/service
COPY --from=builder /workspace/yaza.toml /app/yaza.toml

ENV PORT=8080
EXPOSE 8080

USER nonroot:nonroot
ENTRYPOINT ["/app/service"]
