FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    nlohmann-json3-dev \
    openssl \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build --target edgeshield-proxy -j

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libboost-system1.83.0 libssl3t64 ca-certificates openssl \
  && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/core-proxy/edgeshield-proxy /app/edgeshield-proxy
COPY config/edgeshield.json /app/config/edgeshield.json
RUN mkdir -p /app/certs \
  && openssl req -x509 -nodes -newkey rsa:2048 \
    -keyout /app/certs/edgeshield-dev.key \
    -out /app/certs/edgeshield-dev.crt \
    -days 365 \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
EXPOSE 8080 8443 9090
CMD ["/app/edgeshield-proxy", "/app/config/edgeshield.json"]
