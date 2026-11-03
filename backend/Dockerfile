
# ============================================================
# CHRONOPLAN - PRODUCTION DOCKERFILE
# C++20 + Crow + SQLite + vcpkg
# Deployment target: Render
# ============================================================


# ------------------------------------------------------------
# STAGE 1: BUILD
# ------------------------------------------------------------

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg

# Install Linux build tools and dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    make \
    gcc \
    g++ \
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

# Verify compiler and build tools
RUN gcc --version && \
    g++ --version && \
    cmake --version && \
    make --version

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# Copy dependency manifest
COPY vcpkg.json ./

# Copy backend source code
COPY . .

# ------------------------------------------------------------
# Configure and build ChronoPlan
# ------------------------------------------------------------

RUN cmake -S . -B build -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DCHRONOPLAN_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    || { \
        echo ""; \
        echo "========================================"; \
        echo "       VCPKG CONFIGURATION FAILED"; \
        echo "========================================"; \
        echo ""; \
        echo "VCPKG MANIFEST LOG:"; \
        cat build/vcpkg-manifest-install.log 2>/dev/null || true; \
        echo ""; \
        echo "LIBSODIUM BUILD LOGS:"; \
        find ${VCPKG_ROOT}/buildtrees/libsodium \
            -type f -name "*.log" \
            -exec sh -c 'echo "--- $1 ---"; tail -n 100 "$1"' _ {} \; \
            2>/dev/null || true; \
        exit 1; \
    }

# Compile the application
RUN cmake --build build --parallel 2


# ------------------------------------------------------------
# STAGE 2: PRODUCTION RUNTIME
# ------------------------------------------------------------

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    libsqlite3-0 \
    libcurl4 \
    libsodium23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled ChronoPlan executable
COPY --from=builder /app/build/chronoplan ./chronoplan

# Copy vcpkg runtime libraries
COPY --from=builder /opt/vcpkg/installed/x64-linux/lib /opt/vcpkg/lib
COPY --from=builder /opt/vcpkg/installed/x64-linux/bin /opt/vcpkg/bin

# Configure shared library lookup
ENV LD_LIBRARY_PATH=/opt/vcpkg/lib:/opt/vcpkg/bin

# Render configuration
ENV CHRONOPLAN_BIND_ADDR=0.0.0.0

# Free plan: temporary database storage
ENV CHRONOPLAN_DB_PATH=/tmp/chronoplan.db

# Render provides PORT automatically
EXPOSE 10000

# Start ChronoPlan
CMD ["./chronoplan"]
