#!/usr/bin/env python3

import time

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>Quick Timeout Test (10 seconds)</h1>")

# Sleep for 10 seconds
time.sleep(10)

print("<p>Completed after 10 seconds</p>")
print("</body></html>")