#!/usr/bin/env python3
# Timeout test: sleeps 40s, which exceeds CGI_TIMEOUT_SECONDS (30).
# Server should kill it and return 504.
import time

time.sleep(40)
print("Content-Type: text/plain")
print()
print("you should never see this")
