#!/bin/bash
# Self-executing CGI: webserv has NO interpreter configured for `.cgi`
# (`cgi_extension .cgi;`). The kernel uses this shebang to pick /bin/bash,
# so any language with a shebang can be a CGI type without touching webserv.
printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'interpreter=self-executing via shebang (%s)\n' "$0"
printf 'method=%s\n' "$REQUEST_METHOD"
printf 'query=%s\n' "$QUERY_STRING"
