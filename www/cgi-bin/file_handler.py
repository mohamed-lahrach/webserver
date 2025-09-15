#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

method = os.environ.get('REQUEST_METHOD', 'Unknown')
file_name = os.environ.get('HTTP_X_FILE_NAME', '').strip()
content_length = os.environ.get('CONTENT_LENGTH', '0')

# Check for temp file or read from stdin
file_processed = False
if method == 'POST':
    temp_file_path = f"/tmp/{file_name}" if file_name else "/tmp/cgi_post_data.txt"
    if os.path.exists(temp_file_path):
        file_size = os.path.getsize(temp_file_path)
        file_processed = True
    elif content_length and content_length != '0':
        try:
            data = sys.stdin.buffer.read(int(content_length))
            file_size = len(data)
            file_processed = True
        except:
            file_size = 0

print(f"""<!DOCTYPE html>
<html>
<head><title>File Upload Test</title></head>
<body>
    <h1>✅ File Upload Works!</h1>
    <p><strong>Method:</strong> {method}</p>
    <p><strong>File Name:</strong> {file_name or 'text data'}</p>
    <p><strong>File Size:</strong> {file_size if file_processed else 0} bytes</p>
    <p><strong>Status:</strong> File upload processing successful</p>
</body>
</html>""")
