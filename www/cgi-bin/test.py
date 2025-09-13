#!/usr/bin/env python3

import os

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>Simple CGI Test</h1>")
print(f"<p>Working directory: {os.getcwd()}</p>")
print(f"<p>Request method: {os.environ.get('REQUEST_METHOD', 'Unknown')}</p>")
print("</body></html>")