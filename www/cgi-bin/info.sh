#!/bin/bash
# Shell CGI type. Selected by `cgi_extension .sh /bin/bash;`.
# webserv execve's /bin/bash with this file as argv[1].
printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'interpreter=bash\r\n'
printf 'method=%s\r\n' "$REQUEST_METHOD"
printf 'query=%s\r\n' "$QUERY_STRING"
printf 'script_name=%s\r\n' "$SCRIPT_NAME"
