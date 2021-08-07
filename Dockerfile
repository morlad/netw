FROM alpine:3.14

COPY . /src

WORKDIR /src/build

RUN apk add --no-cache gcc libcurl curl-dev cmake make musl-dev pkgconf && \
    cmake .. && \
    cmake --build .

ENTRYPOINT ["/src/build/example"]
