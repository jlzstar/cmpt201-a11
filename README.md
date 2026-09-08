# Group Chat Server & Fuzzing Clients

A multi-client TCP group chat system written using C. This is a systems programming course project, which consists of a single relay **server** and any number of **client** programs that act as simple fuzzers: each client generates randomized messages, sends them to the server, and logs whatever the server broadcasts back.

## Overview

- **`server`** — accepts an arbitrary number of TCP client connections (tested up to 100), receives messages from any client, and relays each message to every connected client (including the sender) in a globally consistent order.
- **`client`** — connects to the server, generates a configurable number of random messages, sends them, listens for and logs everything the server broadcasts, then completes a coordinated shutdown handshake before terminating.

## Protocol design

Every message on the wire has the shape:

```
[1-byte type] [optional binary header] [payload] '\n'
```

**Type 0 — chat message**

| Direction | Format |
|---|---|
| Client → Server | `0x00` + payload + `\n` |
| Server → Client | `0x00` + 4-byte sender IP (`uint32_t`) + 2-byte sender port (`uint16_t`) + payload + `\n` |

**Type 1 — end-of-execution signal**

Implements a simplified two-phase commit for graceful shutdown:

1. After a client sends its full quota of messages, it sends a type-1 message (`0x01\n`) to the server.
2. The server waits until it has received a type-1 message from *every* connected client.
3. The server then broadcasts a type-1 message to all clients and terminates.
4. Each client terminates upon receiving the type-1 broadcast.

**Framing gotcha:** the IP/port header is raw binary, not text — so it can legally contain a byte equal to `\n`. Message framing can't just scan blindly for the next newline across the whole message; the fixed-size binary header has to be read separately from the newline-delimited text payload that follows it.

## Concurrency model

> Adjust this section to match what you actually implemented — see note below.

- Server: one thread per client connection reads incoming messages; a mutex-protected broadcast path ensures all clients observe messages in one consistent global order regardless of arrival timing.
- Client: concurrent send and receive paths (a fuzzer/sender thread plus a listener thread) so it can generate and transmit random messages while simultaneously logging server broadcasts.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

Produces two executables: `server` and `client`.

## Usage

```bash
./server <port> <# of clients>
./client <server IP> <port> <# of messages> <log file path>
```

Example, using `telnet` as an ad-hoc client to sanity-check the server:

```bash
./server 8000 1
telnet 127.0.0.1 8000
```

Client log lines are written in the format:

```
%-15s%-10u%s
```

e.g. `192.168.68.6      9000      9391DE3E275ADB19637D`

## Testing

Validated against the course-provided `server-tester` and `client-tester` harnesses, covering:

- Single-client message send/receive correctness and protocol formatting
- Message ordering under high volume (up to ~1000 messages per client)
- Many-client broadcast correctness and ordering (up to 100 clients)
- Two-phase-commit termination for one client and for many clients

## Notes / limitations

- Message size is capped at 1024 bytes per the protocol spec.
- Not designed to scale past ~100 concurrent clients.
- Built and tested without debug/sanitizer flags enabled, per submission requirements (development/debugging was done separately with sanitizers enabled).
