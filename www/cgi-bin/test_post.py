#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

content_length = os.environ.get('CONTENT_LENGTH', '0')
if content_length and content_length.isdigit() and int(content_length) > 0:
    post_data = sys.stdin.read(int(content_length))
else:
    post_data = ""

print("<h1>CGI POST Test</h1>")
print(f"<p>Method: {os.environ.get('REQUEST_METHOD', 'UNKNOWN')}</p>")
print(f"<p>Content Length: {content_length}</p>")
print(f"<p>POST Data: {post_data}</p>")