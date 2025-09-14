#!/usr/bin/env python3

print("Content-Type: text/html")
print()
print("<h1>🔄 Infinite Loop Test</h1>")
print("<p>This script will run forever... timeout should kill it after 30 seconds!</p>")

# Force output to be sent
import sys
sys.stdout.flush()

# This will run forever - timeout should kill it
import time
counter = 0
while True:
    time.sleep(1)
    counter += 1
    # Don't print anything - this ensures no EPOLLIN events
