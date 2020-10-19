FROM ubuntu:20.04

WORKDIR /root/

ENV TZ=GMT
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH


COPY extract/bin  /usr/local/bin
COPY extract/lib  /usr/local/lib
COPY extract/lib64 /usr/local/lib64
COPY extract/runDeps .

RUN runDeps=$(cat runDeps) && \
    echo $runDeps && \
    apt-get update && apt-get install -y $runDeps bash

