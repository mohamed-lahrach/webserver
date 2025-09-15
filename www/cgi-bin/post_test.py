#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

method = os.environ.get('REQUEST_METHOD', 'Unknown')
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
    <h1>POST Request Works!</h1>
    <p><strong>Method:</strong> {method}</p>
    <p><strong>Content Length:</strong> {content_length}</p>
    <p><strong>Data Received:</strong> {len(post_data)} bytes</p>
    <p><strong>Status:</strong> POST processing successful</p>
    {f'<pre style="background: #f5f5f5; padding: 10px;">{post_data[:200]}{"..." if len(post_data) > 200 else ""}</pre>' if post_data else ''}
</body>
</html>""")
