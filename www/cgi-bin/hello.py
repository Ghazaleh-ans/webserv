#!/usr/bin/env python3
# Basic GET test: prints env vars the server should have set.
import os
import sys

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>CGI Hello</h1>")
print("<table border='1'>")
for key in sorted(os.environ.keys()):
    print("<tr><td>{}</td><td>{}</td></tr>".format(key, os.environ[key]))
print("</table>")
print("</body></html>")
