#!/bin/bash

printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'interpreter=bash\r\n'
printf 'method=%s\r\n' "$REQUEST_METHOD"
printf 'query=%s\r\n' "$QUERY_STRING"
printf 'script_name=%s\r\n' "$SCRIPT_NAME"
