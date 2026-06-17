
# POLLHUP on the CGI stdout pipe

`POLLHUP` fires on the server's read end of the stdout pipe when the child process
closes its write end (usually on exit). It does **not** mean the pipe buffer is empty —
data may still be sitting in the kernel buffer waiting to be read.

```mermaid
sequenceDiagram
    participant C as CGI Script (child)
    participant B as Kernel pipe buffer
    participant S as Server (parent)

    S->>B: creates pipe, both ends open
    S->>C: fork() + execve() → script starts running

    C->>B: writes "Content-Type: text/html..."
    B-->>S: POLLIN fires → data available → server calls read() → drains buffer

    Note over C: script finishes, process exits
    C->>B: OS seals the write end (no more writers)

    Note over B: write end has zero owners
    B-->>S: POLLHUP fires → "child sealed its end, go check what is left"

    S->>B: calls read() → pulls remaining data out of kernel buffer
    S->>B: calls read() again → returns 0 (buffer empty = EOF)
    S->>B: closes read end → pipe is gone
    Note over S: _stdout_fd = -1 → is_finished() can now return true
```

## Key distinction

| Event | What it means |
|-------|--------------|
| `POLLIN` | Data is in the kernel buffer right now — come read it |
| `POLLHUP` | Child sealed its end — no new data ever, but buffer may still have bytes |
| `read()` returns `0` | Buffer is fully empty — you got everything |

That is why `handle_cgi_event()` triggers `on_readable_stdout()` on **both** `POLLIN`
and `POLLHUP` — after the child exits you still need to drain whatever is left in the
buffer before closing the fd.