# webserv — Architecture Diagrams

All diagrams use [Mermaid](https://mermaid.js.org/) syntax, renderable in GitHub, VSCode with the Mermaid extension, or mermaid.live.

| File | Contents |
|------|----------|
| [01_overview.md](01_overview.md) | End-to-end data flow: TCP connect → HTTP response |
| [02_server_poll_loop.md](02_server_poll_loop.md) | `Server::run()` — main poll loop, fd dispatch, CGI progress, timeouts |
| [03_http_parser.md](03_http_parser.md) | `HttpRequestParser` — 8-state machine + per-state parsing logic |
| [04_client_lifecycle.md](04_client_lifecycle.md) | `Client` — full lifecycle from accept to close |
| [05_router.md](05_router.md) | `Router::route()` — location matching, method check, CGI detection |
| [06_cgi_session.md](06_cgi_session.md) | `CgiSession` — fork/exec, async pipe I/O, timeout, finalization |
| [07_response_builder.md](07_response_builder.md) | `ResponseBuilder` — error / redirect / serve / upload / delete paths |
| [08_sequence_diagram.md](08_sequence_diagram.md) | Sequence diagrams: static file, CGI, timeout, upload, startup/shutdown |
| [09_POLLHUP.md](09_POLLHUP.md) | `POLLHUP` on the CGI stdout pipe — why it fires, kernel buffer behaviour, difference from `POLLIN` and EOF |
| [10_poll_events.md](10_poll_events.md) | `POLLIN`, `POLLOUT`, `POLLHUP` explained for all fds — client socket, CGI stdin/stdout pipes — with sequence diagrams, flow charts, and a full end-to-end CGI event timeline |
