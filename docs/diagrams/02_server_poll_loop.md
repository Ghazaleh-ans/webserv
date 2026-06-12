# Server Poll Loop

`Server::run()` — the main event loop driving all I/O.

```mermaid
flowchart TD
    START["Server::run()"] --> LOOP["while !_stop_requested"]

    LOOP --> BUILD["build_pollfds()\nListeners: POLLIN\nClients: POLLIN + POLLOUT if data\nCGI pipes: POLLIN/POLLOUT"]
    BUILD --> POLL["n = poll(fds, timeout=5000)"]

    POLL --> EINTR{"n < 0?"}
    EINTR -->|EINTR| LOOP
    EINTR -->|other error| LOOP

    POLL --> TIMEOUT{"n == 0?"}
    TIMEOUT -->|Yes| CGI0["check_cgi_progress()"]
    CGI0 --> SWEEP0["sweep_timeouts()"]
    SWEEP0 --> LOOP

    TIMEOUT -->|No| ITER["for each fd with revents != 0"]

    ITER --> TYPE{"fd owner?"}

    TYPE -->|Listener fd| LACCEPT["Listener::accept_one()\n→ new Client()"]
    LACCEPT --> REG["_clients[client_fd] = client"]
    REG --> ITER

    TYPE -->|CGI stdin/stdout fd| CGIEV["handle_cgi_event(fd)"]
    CGIEV --> CGIPOLL{"revents &"}
    CGIPOLL -->|POLLOUT on stdin| WSTDIN["CgiSession::on_writable_stdin()\nsend request body bytes"]
    CGIPOLL -->|POLLIN on stdout| RSTDOUT["CgiSession::on_readable_stdout()\nbuffer CGI output"]
    CGIPOLL -->|POLLHUP/ERR| CLOSEPIPE["close pipe fd\nmark closed"]
    WSTDIN --> ITER
    RSTDOUT --> ITER
    CLOSEPIPE --> ITER

    TYPE -->|Client fd| CLEV["handle_client_event(fd)"]
    CLEV --> ERR{"POLLHUP\nPOLLERR\nPOLLNVAL?"}
    ERR -->|Yes| DROP["drop_client(fd)\ndelete Client\nclose socket"]
    ERR -->|No| RCHECK{"revents & POLLIN?"}
    RCHECK -->|Yes| READ["Client::on_readable()\nrecv() → parser.feed()"]
    RCHECK -->|No| WCHECK
    READ --> PARSE{"parser state?"}
    PARSE -->|STATE_DONE| BUILD_RESP["build_response()\n→ Router + ResponseBuilder\nor new CgiSession"]
    PARSE -->|STATE_ERROR| ERR_RESP["build_error_response(code)"]
    PARSE -->|Parsing| WCHECK
    BUILD_RESP --> WCHECK
    ERR_RESP --> WCHECK
    WCHECK{"revents & POLLOUT?"}
    WCHECK -->|Yes| WRITE["Client::on_writable()\nsend() from _out_buffer"]
    WRITE --> EMPTY{"_out_buffer empty\n& _response_built?"}
    EMPTY -->|Yes| DROP
    EMPTY -->|No| ITER
    WCHECK -->|No| ITER

    DROP --> ITER
    ITER -->|all fds done| CGIPROG["check_cgi_progress()"]
    CGIPROG --> CGIDONE{"cgi->is_finished()?"}
    CGIDONE -->|Yes| FINALIZE["Client::finalize_cgi()\nbuild HTTP response from CGI output"]
    CGIDONE -->|No| CGITIME{"CGI timeout > 30s?"}
    CGITIME -->|Yes| KILL["CgiSession::kill_child()\n→ SIGKILL\nbuild 504 error"]
    CGITIME -->|No| SWEEP
    FINALIZE --> SWEEP
    KILL --> SWEEP
    SWEEP["sweep_timeouts()\nclient idle > 30s → drop_client()"]
    SWEEP --> LOOP
```
