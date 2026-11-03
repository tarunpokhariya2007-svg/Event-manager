
# ---------- Build stage ----------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    autoconf \
    automake \
    libtool \
    python3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# Copy dependency manifests first
COPY vcpkg.json ./

# Copy the backend source
COPY . .

# Build the production application
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DCHRONOPLAN_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2


# ---------- Runtime stage ----------
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    libsqlite3-0 \
    libcurl4 \
    libsodium23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy application and vcpkg runtime libraries
COPY --from=builder /app/build/chronoplan ./chronoplan
COPY --from=builder /opt/vcpkg/installed/x64-linux/lib /opt/vcpkg/lib
COPY --from=builder /opt/vcpkg/installed/x64-linux/bin /opt/vcpkg/bin

ENV LD_LIBRARY_PATH=/opt/vcpkg/lib:/opt/vcpkg/bin

# Render supplies PORT automatically
ENV CHRONOPLAN_BIND_ADDR=0.0.0.0
ENV CHRONOPLAN_DB_PATH=/var/data/chronoplan.db

EXPOSE 10000

CMD ["./chronoplan"]
