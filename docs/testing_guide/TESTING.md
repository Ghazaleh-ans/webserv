# webserv — Testing Guide

A practical, table-based guide for manually testing **webserv**: **what** each test proves, the **curl** command, and the **browser** URL. It covers usual-case and edge-case behaviour, memory-leak checks, siege stress tests, and a curl-flag reference.

> Always inspect the *status line and headers*, not just the body. Use `curl -i` (include response headers) or `curl -v` (full request+response trace) — a page that "looks right" in a browser can still carry a wrong status code or a missing/duplicated header.

Unless noted, tables assume the feature-test config:

```bash
./webserv configs/test.conf     # server A on 127.0.0.1:8080, server B on 0.0.0.0:9090
```

> **Connection model:** this server uses **HTTP/1.0-style connections** — every response carries `Connection: close` and the socket is dropped after it. There is **no keep-alive and no pipelining**, so tests for persistent/reused connections don't apply and are intentionally excluded. (Chunked request decoding and the HTTP/1.1 `Host` requirement *are* implemented and are tested below.)

Routes in `configs/test.conf`:

| Route | Methods | Purpose |
|-------|---------|---------|
| `/` | GET | static site, `index.html`, `autoindex off` |
| `/upload` | POST DELETE | file upload (`upload_store www/uploads`, 10M limit) |
| `/cgi-bin` | GET POST | CGI (`.py` → python3) |
| `/old` | — | `return 301 /new` |
| `/files` | GET | static dir with `autoindex on` |
| `:9090 /` `/api` | GET (DELETE) | second server, custom error pages |

> **Browser column:** only `GET` fits in the address bar. For `POST`/`DELETE`/header tricks, use `curl` (or devtools `fetch`). A `—` means "not doable from the address bar".

---

## 1. Usual-case tests

| # | What it tests | curl command | Browser |
|---|---------------|--------------|---------|
| 1 | Serve static index | `curl -i http://127.0.0.1:8080/` | `http://127.0.0.1:8080/` |
| 2 | Serve a named static file | `curl -i http://127.0.0.1:8080/index.html` | `http://127.0.0.1:8080/index.html` |
| 3 | Correct `Content-Type` (MIME) | `curl -I http://127.0.0.1:8080/files/Max.jpeg` | `http://127.0.0.1:8080/files/Max.jpeg` |
| 4 | Autoindex directory listing | `curl -i http://127.0.0.1:8080/files/` | `http://127.0.0.1:8080/files/` |
| 5 | 301 redirect | `curl -i http://127.0.0.1:8080/old` (see the 301) · `curl -iL http://127.0.0.1:8080/old` (follow to `/new`) | `http://127.0.0.1:8080/old` |
| 6 | CGI GET (query string) | `curl -i "http://127.0.0.1:8080/cgi-bin/greet.py?name=Ghazaleh"` | `http://127.0.0.1:8080/cgi-bin/greet.py?name=Ghazaleh` |
| 7 | CGI POST (body → stdin) | `curl -i -X POST --data "hello=world" http://127.0.0.1:8080/cgi-bin/echo.py` | — |
| 8 | CGI custom `Status:` header | `curl -i http://127.0.0.1:8080/cgi-bin/status.py` | `http://127.0.0.1:8080/cgi-bin/status.py` |
| 9 | Upload a file (raw body) | `curl -i -X POST -H "X-Filename: README.md" --data-binary @README.md http://127.0.0.1:8080/upload` | — |
| 10 | DELETE an uploaded file | `curl -i -X DELETE http://127.0.0.1:8080/upload/README.md` | — |
| 11 | Second server on 9090 | `curl -i http://127.0.0.1:9090/` | `http://127.0.0.1:9090/` |
| 12 | `server_name` virtual host | `curl -i -H "Host: api.example.com" http://127.0.0.1:9090/` | — |
| 13 | HEAD request | `curl -I http://127.0.0.1:8080/` | — |
| 14 | Directory → 301 add trailing slash | `curl -i http://127.0.0.1:8080/files` (no slash → `301` `Location: /files/`) | `http://127.0.0.1:8080/files` |
| 15 | CGI env dump (GET) | `curl -i http://127.0.0.1:8080/cgi-bin/hello.py` | `http://127.0.0.1:8080/cgi-bin/hello.py` |
| 16 | Second-server file route | `curl -i http://127.0.0.1:9090/api/index.html` | `http://127.0.0.1:9090/api/index.html` |

> **Upload note (test 9):** the body must be non-empty or the server returns **400** (`empty body`). If curl can't find the file it silently sends an empty POST — run from the repo root or use an absolute path, and use `-sS` to surface curl's "couldn't read file" warning. `--data-binary` does not send the local filename, so pass `-H "X-Filename: ..."` (or put the name in the URI) to control the saved name. Multipart (`-F`) is **not** implemented, so it's intentionally omitted here.

---

## 2. Edge-case / error tests

| # | What it tests | Expected | curl command | Browser |
|---|---------------|----------|--------------|---------|
| 1 | Not found | `404` | `curl -i http://127.0.0.1:8080/nope.html` | `http://127.0.0.1:8080/nope.html` |
| 2 | Method not allowed | `405` | `curl -i -X POST http://127.0.0.1:8080/` | — |
| 3 | DELETE on GET-only route | `405` | `curl -i -X DELETE http://127.0.0.1:8080/index.html` | — |
| 4 | Body over server `client_max_body_size` | `413` | `head -c 2M /dev/zero \| curl -i -X POST --data-binary @- http://127.0.0.1:8080/cgi-bin/echo.py` (inherits the 1m server cap) | — |
| 5 | Per-location small limit | `413` | `./webserv configs/upload.conf` → `head -c 200 /dev/zero \| curl -i -X POST --data-binary @- http://127.0.0.1:8080/small-upload` | — |
| 6 | Dir, no index, autoindex off | `403` | `curl -i http://127.0.0.1:8080/uploads/` | `http://127.0.0.1:8080/uploads/` |
| 7 | Directory traversal blocked | never serves `/etc/passwd` | `curl -i --path-as-is http://127.0.0.1:8080/../../etc/passwd` | — |
| 8 | Encoded traversal blocked | `403/404` | `curl -i --path-as-is "http://127.0.0.1:8080/%2e%2e/%2e%2e/etc/passwd"` | — |
| 9 | Unknown method | `400/501` | `curl -i -X BREW http://127.0.0.1:8080/` | — |
| 10 | Malformed request line | `400` | `printf 'GET\r\n\r\n' \| nc 127.0.0.1 8080` | — |
| 11 | Missing Host (HTTP/1.1) | `400` | `printf 'GET / HTTP/1.1\r\n\r\n' \| nc 127.0.0.1 8080` | — |
| 12 | Chunked transfer-encoding | `200`, decoded body | `curl -i -X POST -H "Transfer-Encoding: chunked" --data-binary "hello" http://127.0.0.1:8080/cgi-bin/echo.py` | — |
| 13 | Content-Length longer than body | no hang; timeout/`400` | `printf 'POST /upload HTTP/1.1\r\nHost: x\r\nContent-Length: 100\r\n\r\nshort' \| nc 127.0.0.1 8080` | — |
| 14 | CGI timeout | `504` after ~30s | `curl -i http://127.0.0.1:8080/cgi-bin/slow.py` | `http://127.0.0.1:8080/cgi-bin/slow.py` |
| 15 | CGI script missing | `404` | `curl -i http://127.0.0.1:8080/cgi-bin/ghost.py` | — |
| 16 | Custom error page body | `404` w/ `www/errors/404.html` | `curl -i http://127.0.0.1:9090/missing` | `http://127.0.0.1:9090/missing` |
| 17 | `/foo` must NOT match `/foobar` | `/` route, not `/foo` | `./webserv configs/routing.conf` → `curl -i http://127.0.0.1:8080/foobar` | `http://127.0.0.1:8080/foobar` |
| 18 | Empty-body POST | handled, no crash | `curl -i -X POST -H "Content-Length: 0" http://127.0.0.1:8080/upload` | — |
| 19 | Very long URI | `414`, no crash | `curl -i "http://127.0.0.1:8080/$(python3 -c 'print("a"*20000)')"` (≈8 KB request line still 404s; 20000 trips the cap) | — |
| 20 | Partial/slow send (slowloris-lite) | conn times out, server survives | `printf 'GET / HTTP/1.1\r\n' \| nc 127.0.0.1 8080` (leave hanging) | — |
| 21 | Valid percent-decoding | `200` (`%69`→`i`) | `curl -i "http://127.0.0.1:8080/%69ndex.html"` | `http://127.0.0.1:8080/%69ndex.html` |
| 22 | `%00` not truncated | `404` (NUL kept literal) | `curl -i "http://127.0.0.1:8080/index.html%00.txt"` | — |
| 23 | Unsupported HTTP version | `505` | `printf 'GET / HTTP/2.0\r\nHost: x\r\n\r\n' \| nc 127.0.0.1 8080` | — |
| 24 | `Content-Length` + `Transfer-Encoding` (smuggling guard) | `400` | `printf 'POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n' \| nc 127.0.0.1 8080` | — |
| 25 | Oversized header line | `431` | `printf 'GET / HTTP/1.1\r\nHost: x\r\nX-Big: %s\r\n\r\n' "$(head -c 20000 /dev/zero \| tr '\0' A)" \| nc 127.0.0.1 8080` | — |
| 26 | Raw chunked reassembly (telnet) | body = `Webserv!` | `printf 'POST /cgi-bin/echo.py HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWebs\r\n4\r\nerv!\r\n0\r\n\r\n' \| nc 127.0.0.1 8080` | — |
| 27 | DELETE missing file | `404` | `curl -i -X DELETE http://127.0.0.1:8080/upload/ghost.txt` | — |
| 28 | DELETE a directory | `403` | `curl -i -X DELETE http://127.0.0.1:8080/upload/` | — |
| 29 | Per-location limit RAISES server default | `201` (2 MB accepted at 10M `/upload`, though server is 1m) | `head -c 2M /dev/zero \| curl -i -X POST --data-binary @- http://127.0.0.1:8080/upload` | — |
| 30 | CGI exits non-zero with no output | `502` | make `www/cgi-bin/boom.py` (`#!/usr/bin/env python3` + `import sys; sys.exit(1)`), `chmod +x`, then `curl -i http://127.0.0.1:8080/cgi-bin/boom.py` | — |
| 31 | CRLF in upload filename stays encoded (no header injection) | `201`; `%0d%0a` kept literal in `Location:`, no `X-Injected` header | `curl -i --path-as-is -X POST --data-binary x "http://127.0.0.1:8080/upload/foo%0d%0aX-Injected:%20yes"` | — |
| 32 | Every response is `Connection: close` (HTTP/1.0-style) | header present | `curl -sv http://127.0.0.1:8080/ 2>&1 \| grep -i '^< connection'` | — |
| 33 | ⚠ Autoindex not HTML-escaped (**known weakness**) | should escape `<`→`&lt;`; currently emits filenames **raw** | `curl -s http://127.0.0.1:8080/files/ \| grep -i script` (fixture `www/files/<script>evil.txt`) | `http://127.0.0.1:8080/files/` |

---

## 3. Memory-leak checks (valgrind)

Run under valgrind, exercise, then stop with `Ctrl-C` (SIGINT) so graceful shutdown frees everything. **Target: `definitely lost: 0 bytes` and `0 errors`.**

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --error-exitcode=1 \
         ./webserv configs/test.conf
```

| # | What to exercise for leaks | How to drive it (other terminal) |
|---|----------------------------|-----------------------------------|
| 1 | Clean startup + immediate SIGINT | start under valgrind, then `Ctrl-C` right away |
| 2 | Static request lifecycle | `curl http://127.0.0.1:8080/` a few times, then quit |
| 3 | Config parse path | `for c in configs/*.conf; do valgrind ./webserv $c & sleep 1; kill %1; wait; done` |
| 4 | CGI fork/exec/env cleanup | `curl "http://127.0.0.1:8080/cgi-bin/greet.py?name=x"` — env array + pipe fds |
| 5 | CGI timeout kill path | `curl http://127.0.0.1:8080/cgi-bin/slow.py` — child killed at 30s |
| 6 | Upload buffers (raw body) | `curl -X POST -H "X-Filename: r.md" --data-binary @README.md http://127.0.0.1:8080/upload` |
| 7 | 413 rejection path | `head -c 2M /dev/zero \| curl -X POST --data-binary @- http://127.0.0.1:8080/cgi-bin/echo.py` (2 MB > 1m server cap → 413) |
| 8 | Error-response allocation | `curl http://127.0.0.1:9090/missing` |
| 9 | Client aborts mid-request | `printf 'GET / HTTP/1.1\r\n' \| nc 127.0.0.1 8080` then Ctrl-C the nc |
| 10 | Many short-lived connections | `ab -n 500 -c 20 http://127.0.0.1:8080/` then SIGINT |
| 11 | Unterminated header flood (buffer growth) | `{ printf 'GET / HTTP/1.1\r\nHost: x\r\nX-Flood: '; yes A \| tr -d '\n' \| head -c 50000000; } \| nc 127.0.0.1 8080` — RSS must stay flat; expect `431`/`400` or a timeout, not OOM |
| 12 | **File-descriptor leaks** | `--track-fds=yes`: only fds 0/1/2 open at exit |

**Edge cases most likely to leak (focus here):**

| Area | Why it's risky |
|------|----------------|
| CGI env/argv | `char**` env array + argv allocated before `execve`; must free in parent |
| CGI pipe fds | both ends of stdin/stdout pipes must close on success **and** on error |
| CGI killed on timeout | `CgiSession` freed when child SIGKILLed, not just on normal finish |
| Client disconnects mid-CGI | pipe fds + session cleaned when client vanishes while CGI runs |
| 413/400 early reject | request/buffer objects allocated before the size check still get freed |
| Aborted write | `send()` fails / client closes while outbound buffer half-drained |
| SIGINT under load | shutdown while a CGI child runs and a client has a partial response |

---

## 4. Stress testing with `siege`

Install: `sudo apt install siege`. Run in another terminal while the server runs.

| # | What it tests | siege command |
|---|---------------|---------------|
| 1 | Sustained static load | `siege -b -c 50 -t 30S http://127.0.0.1:8080/` |
| 2 | Availability stays ~100% | `siege -b -c 100 -t 1M http://127.0.0.1:8080/index.html` |
| 3 | High concurrency, no hang | `siege -b -c 255 -t 30S http://127.0.0.1:8080/` |
| 4 | Mixed URLs (url file) | `siege -b -c 50 -t 30S -f urls.txt` |
| 5 | CGI under load | `siege -b -c 20 -t 30S "http://127.0.0.1:8080/cgi-bin/greet.py?name=siege"` |
| 6 | Autoindex under load | `siege -b -c 30 -t 20S http://127.0.0.1:8080/files/` |
| 7 | POST upload load | `siege -b -c 20 -t 20S "http://127.0.0.1:8080/upload POST <README.md"` |
| 8 | Error path under load (404) | `siege -b -c 50 -t 20S http://127.0.0.1:8080/nope` |
| 9 | Long soak (mem stability) | `siege -b -c 25 -t 10M http://127.0.0.1:8080/` (watch RSS in `top`) |
| 10 | fd leak after a run | run test 1, then `lsof -p $(pgrep webserv) \| wc -l` back to baseline |

`urls.txt` for test 4:
```
http://127.0.0.1:8080/
http://127.0.0.1:8080/index.html
http://127.0.0.1:8080/files/
http://127.0.0.1:8080/cgi-bin/greet.py?name=a
http://127.0.0.1:9090/
```

**What to look for in siege output:**

| Metric | Goal |
|--------|------|
| Availability | `100.00 %` — no dropped connections |
| Failed transactions | `0` — the subject's key metric |
| Longest transaction | not seconds-long (a spike means a hang/block) |
| Server RSS / fd count | flat after the run (no leak under load) |

> Use `-b` (benchmark: no delay) for max pressure; drop it for realistic pacing. Keep `-c` ≤ 255 (siege's default fd ceiling).

---

## 5. Useful `curl` flags

| Flag | Meaning |
|------|---------|
| `-i` | Include response headers in output |
| `-I` | Send `HEAD`, headers only |
| `-v` | Verbose: request + response headers, connection info |
| `-L` | Follow redirects — re-request the `Location:` of a 3xx (combine as `-iL` to see each hop's headers) |
| `--trace-ascii -` | Full ascii dump of the whole exchange |
| `-X <METHOD>` | Set request method (`POST`, `DELETE`, `BREW`, …) |
| `-H "Header: val"` | Add/override a request header (`Host:`, `Content-Type:`, …) |
| `-d / --data` | POST body (sets `application/x-www-form-urlencoded`) |
| `--data-binary @file` | POST raw file bytes, no newline mangling (`@-` = stdin) |
| `-F "name=@file"` | multipart/form-data file upload |
| `-T file` | PUT/upload a file |
| `--path-as-is` | Don't let curl normalize `../` — required for traversal tests |
| `-o file` / `-O` | Write body to file / to remote filename |
| `-s` / `-S` | Silent / show errors even when silent |
| `-w "%{http_code}\n"` | Print info after transfer (status, timings, sizes) |
| `--http1.0` / `--http1.1` | Force protocol version (handy to send a 1.0 request without `Host`) |
| `--limit-rate 1k` | Throttle transfer (simulate slow client) |
| `-m <secs>` | Max total time (catch hangs) |
| `--connect-timeout <secs>` | Max time to connect |
| `-A "<agent>"` | Set `User-Agent` |
| `-b "k=v"` / `-c file` | Send cookies / save cookies |
| `-x host:port` | Route through a proxy |
| `-k` | Skip TLS verification (if TLS is added) |
| `-#` | Progress bar instead of meter |

Print just the status code:
```bash
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8080/
```
