# Running with Docker (Linux + Windows)

Same image runs on both platforms so you and your partner get identical build and runtime. The image is based on Alpine Linux for a small footprint.

## Build the image

From the project root (where `Dockerfile` lives):

```bash
docker build -t cis427-pa1 .
```

(On Windows use the same in PowerShell or WSL.)

## Run the server

**Option A – docker run**

```bash
docker run -p 5432:5432 -v "$(pwd)/users_and_stocks.db:/app/users_and_stocks.db" cis427-pa1
```

Windows PowerShell: use `"${PWD}\users_and_stocks.db:/app/users_and_stocks.db"` for the volume.

**Option B – docker compose**

```bash
docker compose up
```

Server listens on port 5432. The DB file is stored on the host so it persists.

## Run the client

- **On your machine:** Build locally (`g++ -std=c++11 -o client.bin client.cpp`) and run `./client.bin localhost` (or the server’s IP/hostname).
- **Or from the same image:**  
  `docker run --rm -it --network host cis427-pa1 ./client.bin localhost`  
  (On Windows, replace `--network host` with connecting to the host’s IP, e.g. `./client.bin host.docker.internal` if the server runs on the host.)

## Partner runs server, you run client

Partner (Windows): `docker compose up` (or `docker run ...` and expose 5432).  
You (Linux): run `./client.bin <partner-ip-or-hostname>` (or use the image as above with their IP).  
Firewall must allow TCP 5432.

## Run client in a second container (same host)

```bash
docker compose up -d
docker run --rm -it --network container:cis427_pa1-server-1 cis427-pa1 ./client.bin 127.0.0.1
```

(Adjust the container name if compose names it differently; use `docker ps` to check.)
