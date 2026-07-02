#!/usr/bin/env python3
# Status header test: emits a non-200 Status header
# Server must pass it through as the HTTP status line
print("Status: 404 Not Found")
print("Content-Type: text/plain")
print()
print("custom 404 from CGI")
