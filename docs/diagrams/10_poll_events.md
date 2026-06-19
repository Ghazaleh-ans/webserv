# Poll Events: POLLIN, POLLOUT, POLLHUP

## What is poll()?

`poll()` is a system call that lets the server **wait** for activity on multiple file
descriptors at once, without wasting CPU in a busy loop.

Think of it like a hotel receptionist watching many phones at once. Instead of
constantly asking "is there a call?", they just sit and wait — the moment any phone
rings, they handle it.

```mermaid
flowchart TD
    A[Server calls poll] --> B{Any fd has activity?}
    B -- No --> A
    B -- Yes --> C[Handle the active fd]
    C --> A
```

---

## The Three Events

| Event | Meaning | Analogy |
|-------|---------|---------|
| `POLLIN` | Data is ready to **read** | Your mailbox has letters in it |
| `POLLOUT` | Space is available to **write** | Your outbox has room for more letters |
| `POLLHUP` | The other side **hung up** / closed the connection | The other person put down the phone |

---

## Who Has What File Descriptors?

```mermaid
flowchart LR
    subgraph Server
        S_CLIENT_FD[client_fd\ntcp socket]
        S_STDIN[_stdin_fd\nstdin_pipe write end]
        S_STDOUT[_stdout_fd\nstdout_pipe read end]
    end

    subgraph CGI Child
        C_STDIN[fd 0 stdin\nstdin_pipe read end]
        C_STDOUT[fd 1 stdout\nstdout_pipe write end]
    end

    subgraph HTTP Client
        BROWSER[Browser / curl]
    end

    BROWSER -- TCP --> S_CLIENT_FD
    S_STDIN -- pipe --> C_STDIN
    C_STDOUT -- pipe --> S_STDOUT
```

---

## POLLIN — Data is Ready to Read

### What triggers it?
The kernel raises POLLIN on a fd when bytes appear in its buffer.

### On `client_fd` (HTTP client → Server)
```mermaid
sequenceDiagram
    participant B as Browser
    participant K as Kernel TCP buffer
    participant S as Server

    B->>K: sends "GET /index.html HTTP/1.1..."
    Note over K: bytes arrive in TCP buffer
    K-->>S: POLLIN fires on client_fd
    S->>K: read(client_fd) → gets the HTTP request
```

**In code:** `Server.cpp` — `if (revents & POLLIN)` on the client socket,
the server reads the incoming HTTP request.

### On `_stdout_fd` (CGI → Server)
```mermaid
sequenceDiagram
    participant CGI as CGI Script
    participant K as Kernel pipe buffer
    participant S as Server

    CGI->>K: printf("Content-Type: text/html\n\nhello")
    Note over K: bytes appear in stdout pipe buffer
    K-->>S: POLLIN fires on _stdout_fd
    S->>K: read(_stdout_fd) → gets CGI output chunk
    S->>S: appends chunk to _stdout_buf
```

**In code:** `CgiSession.cpp` — `read(_stdout_fd, buf, sizeof(buf))`
inside `on_readable_stdout()`.

### Summary table

| fd | Who writes into it | Who gets POLLIN |
|----|-------------------|-----------------|
| `client_fd` | Browser / HTTP client | Server |
| `_stdout_fd` | CGI child (its fd 1) | Server |

---

## POLLOUT — Space Available to Write

### What triggers it?
The kernel raises POLLOUT when the buffer has room and you can write without blocking.

### On `client_fd` (Server → Browser)
```mermaid
sequenceDiagram
    participant S as Server
    participant K as Kernel TCP buffer
    participant B as Browser

    Note over S: has HTTP response in _out_buffer
    K-->>S: POLLOUT fires on client_fd
    S->>K: write(client_fd, response_data)
    K->>B: TCP delivers the response
```

**In code:** `Server.cpp` — `if (revents & POLLOUT)` on client socket,
server writes from `_out_buffer` to the client.

### On `_stdin_fd` (Server → CGI)
```mermaid
sequenceDiagram
    participant S as Server
    participant K as Kernel pipe buffer
    participant CGI as CGI Script

    Note over S: has POST body in _body_to_write
    K-->>S: POLLOUT fires on _stdin_fd
    S->>K: write(_stdin_fd, post_body_chunk)
    CGI->>K: read(stdin) → gets POST data
```

**In code:** `CgiSession.cpp` — `write(_stdin_fd, ...)` inside
`on_writable_stdin()`. Keeps firing until all POST body is sent, then
`_stdin_fd` is closed to signal EOF to the CGI.

### Why not always ask for POLLOUT?

If the server registers POLLOUT even when it has nothing to send, `poll()` returns
immediately every loop — burning 100% CPU doing nothing. So POLLOUT is only
registered when there is actually data waiting to be written.

```mermaid
flowchart TD
    A{Has data to send?} -- Yes --> B[Register POLLOUT\nwait for space]
    A -- No --> C[Do NOT register POLLOUT\nno busy loop]
    B --> D[POLLOUT fires\nwrite data]
    D --> A
```

### Summary table

| fd | Who fills the buffer | Who gets POLLOUT |
|----|---------------------|-----------------|
| `client_fd` | Server (`_out_buffer`) | Server |
| `_stdin_fd` | Server (`_body_to_write`) | Server |

---

## POLLHUP — The Other Side Closed

### What triggers it?
When all writers on a pipe close their end, the kernel marks the read end with POLLHUP.
On a TCP socket, it fires when the remote peer closes the connection.

### On `client_fd` (Browser disconnects)
```mermaid
sequenceDiagram
    participant B as Browser
    participant K as Kernel TCP
    participant S as Server

    Note over B: user closes tab / connection timeout
    B->>K: sends TCP FIN
    K-->>S: POLLHUP fires on client_fd
    S->>S: close(client_fd), remove client from map
```

**In code:** `Server.cpp` — `if (revents & (POLLHUP | POLLERR | POLLNVAL))`
on client socket → client is removed.

### On `_stdout_fd` (CGI exits)
```mermaid
sequenceDiagram
    participant CGI as CGI Script
    participant K as Kernel pipe buffer
    participant S as Server

    Note over CGI: script finishes, process exits
    CGI->>K: OS closes stdout_pipe write end
    Note over K: no more writers on this pipe
    K-->>S: POLLHUP fires on _stdout_fd
    S->>K: read(_stdout_fd) → drains remaining bytes
    S->>K: read(_stdout_fd) → returns 0 (EOF, buffer empty)
    S->>S: close(_stdout_fd), _stdout_fd = -1
    S->>S: finalize_cgi() → build response → send to browser
```

**In code:** `Server.cpp` — POLLHUP on `stdout_fd` triggers
`on_readable_stdout()` which drains the pipe then sets `_stdout_fd = -1`.

### On `_stdin_fd` (CGI closed its stdin early)
```mermaid
sequenceDiagram
    participant S as Server
    participant K as Kernel pipe buffer
    participant CGI as CGI Script

    Note over CGI: script ignores stdin, closes fd 0 early
    CGI->>K: close(stdin fd 0)
    K-->>S: POLLHUP fires on _stdin_fd
    S->>S: on_writable_stdin() → gets EPIPE → closes _stdin_fd
    Note over S: stop trying to write POST body
```

**In code:** `Server.cpp` — POLLHUP on `_stdin_fd` → calls
`on_writable_stdin()` which gets `n <= 0` and closes the fd gracefully.

### Summary table

| fd | What causes POLLHUP | What server does |
|----|--------------------|--------------------|
| `client_fd` | Browser disconnects | Remove client, free memory |
| `_stdout_fd` | CGI exits | Drain pipe, build response, send to browser |
| `_stdin_fd` | CGI closes stdin early | Stop writing POST body, continue normally |

---

## Full Event Flow: CGI Request End to End

```mermaid
sequenceDiagram
    participant B as Browser
    participant S as Server
    participant CGI as CGI Script

    B->>S: TCP connect → POLLIN on listener fd → accept()
    B->>S: sends HTTP POST → POLLIN on client_fd → parse request

    Note over S: creates CgiSession, forks child
    Note over S: registers _stdin_fd + _stdout_fd with poll

    S->>CGI: POLLOUT on _stdin_fd → write POST body chunk
    S->>CGI: POLLOUT on _stdin_fd → write remaining chunks
    S->>CGI: all written → close(_stdin_fd) → CGI sees EOF

    CGI->>S: processes request, writes response
    CGI->>S: POLLIN on _stdout_fd → read chunk → append to _stdout_buf
    CGI->>S: POLLIN on _stdout_fd → read more chunks

    Note over CGI: script exits
    CGI->>S: POLLHUP on _stdout_fd → drain pipe → EOF

    Note over S: finalize_cgi() → build HTTP response
    S->>B: POLLOUT on client_fd → write HTTP response
    B->>S: POLLHUP on client_fd → browser closed → cleanup
```

---

## All Events at a Glance

```mermaid
flowchart LR
    subgraph poll events
        PI[POLLIN\nread data]
        PO[POLLOUT\nwrite data]
        PH[POLLHUP\nother side closed]
    end

    subgraph client_fd
        PI -- browser sent request --> R1[read HTTP request]
        PO -- server has response --> W1[write HTTP response]
        PH -- browser disconnected --> C1[remove client]
    end

    subgraph _stdin_fd
        PO -- pipe has space --> W2[write POST body to CGI]
        PH -- CGI closed stdin --> C2[stop writing]
    end

    subgraph _stdout_fd
        PI -- CGI wrote output --> R2[read CGI response chunk]
        PH -- CGI exited --> C3[drain + finalize]
    end
```

| Event | `client_fd` | `_stdin_fd` | `_stdout_fd` |
|-------|------------|------------|-------------|
| `POLLIN` | Read HTTP request from browser | — | Read CGI output chunk |
| `POLLOUT` | Write HTTP response to browser | Write POST body to CGI | — |
| `POLLHUP` | Browser disconnected → cleanup | CGI closed stdin early → stop writing | CGI exited → drain pipe → finalize |
