#!/usr/bin/env python3

import time
import sys

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>Slow Response Test</h1>")

# Send partial response, then delay
for i in range(10):
    print(f"<p>Step {i+1}: Processing...</p>")
    sys.stdout.flush()
    time.sleep(2)  # 2 second delay between each step

print("<p>Completed after 20 seconds total</p>")
print("</body></html>")