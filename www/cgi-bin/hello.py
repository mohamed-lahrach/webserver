#!/usr/bin/env python3

import os

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Working Directory Test</title></head>")
print("<body>")
print("<h1>Working Directory Test</h1>")
print(f"<p><strong>Current working directory:</strong> {os.getcwd()}</p>")
print(f"<p><strong>Script location:</strong> {os.path.abspath(__file__)}</p>")

# Try to access a file in the same directory
try:
    with open("test.py", "r") as f:
        print("<p><strong>Can access test.py:</strong> Yes</p>")
except FileNotFoundError:
    print("<p><strong>Can access test.py:</strong> No (FileNotFoundError)</p>")

# Try to access a file in the www directory
try:
    with open("index.html", "r") as f:
        print("<p><strong>Can access index.html:</strong> Yes</p>")
except FileNotFoundError:
    print("<p><strong>Can access index.html:</strong> No (FileNotFoundError)</p>")

# Try to access a file using relative path from www
try:
    with open("www/index.html", "r") as f:
        print("<p><strong>Can access www/index.html:</strong> Yes</p>")
except FileNotFoundError:
    print("<p><strong>Can access www/index.html:</strong> No (FileNotFoundError)</p>")

print("</body>")
print("</html>")