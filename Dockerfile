FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive

# Install package dependencies
COPY packages.txt ./
RUN apt-get update \
    && apt-get install --no-install-recommends -y $(cat ./packages.txt) \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ceti-firmware
ENTRYPOINT [ "/bin/bash", "-c" ]
