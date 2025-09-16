#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

method = os.environ.get('REQUEST_METHOD', 'Unknown')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')

# Read POST data if available
post_data = ""
if method == 'POST' and content_length and content_length != '0':
    try:
        length = int(content_length)
        post_data = sys.stdin.read(length)
    except:
        post_data = "Error reading data"

print(f"""<!DOCTYPE html>
<html>
<head><title>POST Test</title></head>
<body>
    <h1>Status: POST Request Successful</h1>
    <p><strong>Method:</strong> {method}</p>
    <p><strong>Query:</strong> {query_string if query_string else '(none)'}</p>
    <p><strong>Data:</strong> {post_data if post_data else '(none)'}</p>
</body>
</html>""")
