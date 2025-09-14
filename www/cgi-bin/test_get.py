#!/usr/bin/env python3

import os

print("Content-Type: text/html")
print()
print("<h1>CGI GET Test</h1>")
print(f"<p>Method: {os.environ.get('REQUEST_METHOD', 'UNKNOWN')}</p>")
print(f"<p>Query: {os.environ.get('QUERY_STRING', 'None')}</p>")