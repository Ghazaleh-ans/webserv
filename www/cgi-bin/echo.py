#!/usr/bin/env python3
# POST test: reads stdin and echoes body + query string back.
import os
import sys

method = os.environ.get("REQUEST_METHOD", "")
length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.read(length) if length > 0 else ""
query = os.environ.get("QUERY_STRING", "")

print("Content-Type: text/plain")
print()
print("method={}".format(method))
print("query={}".format(query))
print("body={}".format(body))
