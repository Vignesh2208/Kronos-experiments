FROM ubuntu:16.04
RUN apt-get update
# just needed since v2
RUN apt-get install -y \
      bison \
              ca-certificates \
              cmake \
              git \
              libssl-dev

RUN apt-get install -y g++ make binutils autoconf automake autotools-dev libtool pkg-config \
  zlib1g-dev libcunit1-dev libssl-dev libxml2-dev libev-dev libevent-dev libjansson-dev \
  libc-ares-dev libjemalloc-dev libsystemd-dev \
  cython python3-dev python-setuptools libboost-dev libboost-thread-dev

RUN mkdir /nghttp2 && git init nghttp2
WORKDIR /nghttp2

ENV URL      https://github.com/nghttp2/nghttp2.git
ENV VERSION  tags/v1.39.2

RUN git fetch --depth 1 $URL $VERSION
RUN git checkout FETCH_HEAD

RUN apt-get update && \
    apt-get install build-essential software-properties-common -y && \
    add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
    apt-get update && \
    apt-get install gcc-6 g++-6 -y && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6

RUN git submodule update --init && autoreconf -i && automake && autoconf && ./configure --enable-asio-lib && make && make install

RUN ldconfig
RUN mkdir /home/nghttp2
WORKDIR /home/nghttp2

COPY client_http2.cc /home/nghttp2
COPY Makefile /home/nghttp2

RUN make build

CMD /home/nghttp2/client_http2 ${SERVER_IP} ${NUM_TRIALS} ${PERIOD}
