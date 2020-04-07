FROM vign2208/http2_client

WORKDIR /home/nghttp2

COPY client_http11.cc /home/nghttp2
COPY Makefile /home/nghttp2

RUN make build

CMD /home/nghttp2/client_http11 ${SERVER_IP} ${NUM_TRIALS} ${PERIOD} ${NUM_THREADS}
