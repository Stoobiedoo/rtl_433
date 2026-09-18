# Build rtl_433 from this checkout. Matches the runtime the hertzg image
# provides: librtlsdr for USB dongles, openssl for -F influx/https.
FROM debian:bookworm-slim AS build
RUN apt-get update -q && apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config \
        librtlsdr-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_RTLSDR=ON -DENABLE_SOAPYSDR=OFF \
    && cmake --build build -j"$(nproc)" \
    && cmake --install build --prefix /out

FROM debian:bookworm-slim
RUN apt-get update -q && apt-get install -y --no-install-recommends \
        librtlsdr0 libssl3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /out/bin/rtl_433 /usr/local/bin/rtl_433
ENTRYPOINT ["rtl_433"]
