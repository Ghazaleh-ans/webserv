#!/usr/bin/env python3
import os

query = os.environ.get("QUERY_STRING", "")
params = {}
for part in query.split("&"):
    if "=" in part:
        k, v = part.split("=", 1)
        params[k] = v

name = params.get("name", "World")

print("Content-Type: text/html\r")
print("\r")
print(f"<html><body><h1>Hello, {name}!</h1></body></html>")