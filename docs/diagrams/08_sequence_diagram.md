# Full Request–Response Sequence Diagram

End-to-end sequence for a static file request, a CGI request, and an upload.

## Static File Request (GET /index.html)

```mermaid
sequenceDiagram
    participant Browser
    participant Listener
    participant Server
    participant Client
    participant Parser as HttpRequestParser
    participant Router
    participant Builder as ResponseBuilder

    Browser->>Listener: TCP connect()
    Listener->>Server: accept_one() → client_fd
    Server->>Client: new Client(client_fd, configs)
    Server->>Server: add to _clients map, register POLLIN

    loop recv loop
        Browser->>Server: send HTTP request bytes
        Server->>Client: on_readable() [POLLIN event]
        Client->>Client: recv(fd, buf, 8192)
        Client->>Parser: feed(buf, n)
        Parser-->>Client: STATE_DONE (or partial)
    end

    Client->>Router: route(request, server_config)
    Router->>Router: match_location() longest prefix
    Router->>Router: method_allowed()
    Router->>Router: build_fs_path()
    Router-->>Client: RouteDecision{KIND_SERVE, fs_path}

    Client->>Builder: build(request, decision, config)
    Builder->>Builder: path_within_root() safety check
    Builder->>Builder: open + read file
    Builder->>Builder: MimeTypes::get_type(ext)
    Builder->>Builder: make_response(200, mime, content)
    Builder-->>Client: HTTP response string

    Client->>Client: _out_buffer = response\n_response_built = true

    loop send loop
        Server->>Client: on_writable() [POLLOUT event]
        Client->>Client: send(fd, _out_buffer)
        Client->>Client: erase sent bytes
    end

    Client->>Client: _out_buffer empty → _should_close = true
    Server->>Server: drop_client(fd)
    Server->>Browser: TCP close
```

## CGI Request (POST /cgi-bin/form.py)

```mermaid
sequenceDiagram
    participant Browser
    participant Server
    participant Client
    participant Parser as HttpRequestParser
    participant Router
    participant CGI as CgiSession
    participant Script as Python Script

    Browser->>Server: TCP connect + send POST request
    Server->>Client: on_readable() events until STATE_DONE
    Client->>Parser: feed() → STATE_DONE

    Client->>Router: route(request, config)
    Router->>Router: match_location() → /cgi-bin
    Router->>Router: extension .py in cgi_handlers?
    Router->>Router: stat(fs_path) — file exists?
    Router-->>Client: RouteDecision{KIND_CGI, /usr/bin/python3}

    Client->>CGI: new CgiSession(request, script, interpreter)
    CGI->>CGI: build_env() — SCRIPT_FILENAME, REQUEST_METHOD\nQUERY_STRING, HTTP_* headers...
    CGI->>CGI: pipe(stdin_pipe), pipe(stdout_pipe)
    CGI->>Script: fork() + execve(python3, script, env)
    CGI-->>Client: constructor returns (_stdin_fd, _stdout_fd)

    Client-->>Server: _cgi set, return from build_response()
    Server->>Server: register_cgi_fds(client, cgi)\nadd pipe fds to _cgi_fd_to_client map

    par Async pipe I/O
        loop write request body to CGI stdin
            Server->>CGI: on_writable_stdin() [POLLOUT on stdin_fd]
            CGI->>Script: write(stdin_fd, request.body)
        end
        loop read CGI output from stdout
            Server->>CGI: on_readable_stdout() [POLLIN on stdout_fd]
            Script->>CGI: write to stdout_pipe
            CGI->>CGI: _stdout_buf += data
        end
    end

    Script->>Script: process exits
    CGI->>CGI: check_child() waitpid(WNOHANG)\n_child_exited = true

    Note over CGI: is_finished():\n_child_exited && _stdout_closed && body sent

    Server->>Client: finalize_cgi()
    Client->>CGI: was_killed()? → No
    Client->>CGI: build_response()\nparse CGI headers from _stdout_buf\nassemble HTTP response
    CGI-->>Client: HTTP response string
    Client->>Client: _out_buffer = response\n_response_built = true
    Client->>Client: delete _cgi

    loop send loop
        Server->>Client: on_writable() [POLLOUT]
        Client->>Browser: send response bytes
    end

    Server->>Server: drop_client(fd)
```

## CGI Timeout (> 30 seconds)

```mermaid
sequenceDiagram
    participant Server
    participant Client
    participant CGI as CgiSession
    participant Script as Hung Script

    Server->>CGI: CgiSession started
    Script->>Script: (hangs, does not exit)

    loop poll iterations, ~30 seconds pass
        Server->>CGI: check_cgi_progress()
        CGI->>CGI: check_child() — not exited
        CGI->>CGI: now - _start_time > 30s? → Yes
        CGI->>Script: kill_child() → SIGKILL
        CGI->>CGI: waitpid cleanup\n_killed_by_timeout = true
    end

    Server->>Client: finalize_cgi()
    Client->>CGI: was_killed()? → Yes
    Client->>Client: _out_buffer = 504 Gateway Timeout
    Client->>Client: _response_built = true
    Server->>Client: drop_client after send
```

## File Upload (POST /upload with multipart/form-data)

```mermaid
sequenceDiagram
    participant Browser
    participant Server
    participant Client
    participant Parser as HttpRequestParser
    participant Router
    participant Builder as ResponseBuilder
    participant UploadHandler
    participant MultipartParser

    Browser->>Server: POST /upload HTTP/1.1\nContent-Type: multipart/form-data; boundary=XYZ\nContent-Length: N\n\n--XYZ\nContent-Disposition: form-data; name="file"; filename="cat.jpg"\n\n<binary data>--XYZ--

    Server->>Client: on_readable() events
    Client->>Parser: feed() — STATE_BODY_LENGTH\n(Content-Length → read N bytes into body)
    Parser-->>Client: STATE_DONE

    Client->>Router: route(POST /upload, config)
    Router->>Router: match_location() → /upload\nmethod POST allowed\nbuild_fs_path() → www/uploads
    Router-->>Client: RouteDecision{KIND_SERVE, www/uploads/}

    Client->>Builder: build(request, decision)
    Builder->>Builder: fs_path is directory + POST?
    Builder->>UploadHandler: handle(request, upload_store, body_limit)

    UploadHandler->>UploadHandler: Content-Type: multipart/form-data?
    UploadHandler->>MultipartParser: parse(body, boundary)
    MultipartParser->>MultipartParser: split parts by boundary\nextract Content-Disposition\nget filename

    MultipartParser-->>UploadHandler: UploadPart{filename, data}
    UploadHandler->>UploadHandler: sanitize_filename()\ncheck body_limit
    UploadHandler->>UploadHandler: open(upload_store/filename, write)\nwrite part.data
    UploadHandler-->>Builder: UploadResult{201, "cat.jpg"}

    Builder->>Builder: make_response(201, text/html, "Uploaded: cat.jpg")
    Builder-->>Client: HTTP/1.1 201 Created ...

    Client->>Client: _out_buffer = response
    Server->>Client: on_writable() → send to browser
    Browser->>Browser: receives 201 Created
```

## Server Startup and Shutdown

```mermaid
sequenceDiagram
    participant OS
    participant main
    participant Server
    participant Listener
    participant SocketUtils

    main->>main: parse argv[1] config path
    main->>main: signal(SIGPIPE, SIG_IGN)
    main->>main: signal(SIGINT, handler → Server::request_stop())
    main->>main: signal(SIGTERM, handler → Server::request_stop())

    main->>main: ConfigParser::parse_file(path)\n→ vector<ServerConfig>

    main->>Server: new Server(configs)
    Server->>Server: start()

    loop for each ServerConfig
        Server->>Listener: new Listener(host, port)
        Listener->>SocketUtils: make_listener(host, port)
        SocketUtils->>OS: socket(AF_INET, SOCK_STREAM)
        SocketUtils->>OS: setsockopt(SO_REUSEADDR)
        SocketUtils->>OS: bind(host:port)
        SocketUtils->>OS: listen(SOMAXCONN)
        SocketUtils->>OS: fcntl(O_NONBLOCK | FD_CLOEXEC)
        SocketUtils-->>Listener: listening fd
        Listener-->>Server: Listener object
    end

    Server->>Server: run() — enter poll loop

    Note over OS,Server: ... server handles requests ...

    OS->>main: SIGINT / SIGTERM
    main->>Server: request_stop()\n_stop_requested = true

    Server->>Server: poll loop exits
    Server->>Server: destructor:\n  close all client fds\n  close all listener fds\n  delete all Client objects
    Server-->>main: run() returns
    main->>OS: exit(0)
```
