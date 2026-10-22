# The document server in a container (examples/server/README.md). Build from the repository's root:
#     docker build -f examples/server/Dockerfile -t affrescoui-server .
# Run it with a folder of documents mounted at /data, behind a TLS proxy (examples/server/deploy/):
#     docker run -v "$PWD/data:/data" -p 127.0.0.1:7420:7420 affrescoui-server
# A token, printed once:
#     docker run --rm -v "$PWD/data:/data" affrescoui-server --folder /data --add-token ada

FROM ubuntu:24.04 AS build
# The packages of .github/actions/setup: the library's configure step finds the windowing ones even for a headless target
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates git cmake ninja-build clang-18 \
      libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev \
      libwayland-dev libxkbcommon-dev wayland-protocols libssl-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /source
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-18 \
      -DCMAKE_C_COMPILER=clang-18 -DAFFRESCOUI_BUILD_TESTS=OFF -DAFFRESCOUI_BUILD_TOOLS=OFF \
    && cmake --build build --target affrescoui_server

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libssl3 ca-certificates \
    && rm -rf /var/lib/apt/lists/* && useradd --system --home /data affresco
COPY --from=build /source/build/affrescoui_server /usr/local/bin/affrescoui_server
USER affresco
VOLUME /data
EXPOSE 7420
# Inside the container the proxy's connections don't come from its loopback, hence --allow-remote: publish the port
# on the host's loopback only (as above), and keep TLS in the proxy
ENTRYPOINT ["affrescoui_server"]
CMD ["--folder", "/data", "--listen", "0.0.0.0:7420", "--allow-remote"]
