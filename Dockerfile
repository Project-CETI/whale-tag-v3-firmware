FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive

# Install package dependencies
RUN apt-get update && apt-get install --no-install-recommends -y dos2unix
COPY packages.txt ./
RUN dos2unix packages.txt
RUN apt-get update && apt-get install --no-install-recommends -y $(cat ./packages.txt)

WORKDIR /ceti-firmware
ENTRYPOINT [ "/bin/bash", "-c" ]
