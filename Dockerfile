# Same environment on Linux and Windows (Docker). Alpine for smaller image.
FROM alpine:3.19

RUN apk add --no-cache \
    build-base \
    sqlite-dev

WORKDIR /app

# Copy source (binaries and db excluded via .dockerignore)
COPY server.cpp utils.cpp utils.hpp client.cpp ./

# Build server and client
RUN g++ -std=c++11 -Wall -Wextra -o server.bin server.cpp utils.cpp -lsqlite3 \
    && g++ -std=c++11 -Wall -Wextra -o client.bin client.cpp

EXPOSE 5432

# Default: run server. Override to run client: docker run --rm -it <image> ./client.bin <host>
CMD ["./server.bin"]
