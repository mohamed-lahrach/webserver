#!/usr/bin/env python3

import time
import sys

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>CGI Timeout Test</h1>")
print("<p>This script will sleep for 30 seconds...</p>")
print("<p>Server should timeout before this completes.</p>")

# Flush output to ensure headers are sent
sys.stdout.flush()

# Sleep for 30 seconds (most servers timeout around 5-10 seconds)
time.sleep(30)

print("<p>If you see this, the server waited 30 seconds!</p>")
print("</body></html>")