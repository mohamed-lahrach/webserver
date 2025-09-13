#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>File-based POST Test</title></head>")
print("<body>")
print("<h1>Testing File-based POST Data</h1>")

# Display request method
method = os.environ.get('REQUEST_METHOD', 'Unknown')
print(f"<p><strong>Request Method:</strong> {method}</p>")

# Try to read from our file (this simulates what happens with real POST)
try:
    with open('/tmp/cgi_post_data.txt', 'r') as f:
        file_data = f.read()
    
    print(f"<p><strong>Data from file:</strong></p>")
    print("<pre style='background: #f5f5f5; padding: 10px; border: 1px solid #ddd;'>")
    print(file_data if file_data else "(empty)")
    print("</pre>")
    
    print(f"<p><strong>Data length:</strong> {len(file_data)} bytes</p>")
    
except FileNotFoundError:
    print("<p><strong>No POST data file found</strong></p>")
except Exception as e:
    print(f"<p><strong>Error reading file:</strong> {str(e)}</p>")

# Also try reading from stdin (normal CGI way)
content_length = os.environ.get('CONTENT_LENGTH', '0')
print(f"<p><strong>Content Length from environment:</strong> {content_length}</p>")

if method == 'POST' and content_length and content_length != '0':
    try:
        length = int(content_length)
        print(f"<p><strong>Attempting to read {length} bytes from stdin...</strong></p>")
        
        # Read the POST data from stdin
        stdin_data = sys.stdin.read(length)
        
        print(f"<p><strong>Data from stdin:</strong></p>")
        print("<pre style='background: #fff8dc; padding: 10px; border: 1px solid #ddd;'>")
        print(stdin_data if stdin_data else "(empty)")
        print("</pre>")
        
    except Exception as e:
        print(f"<p><strong>Error reading from stdin:</strong> {str(e)}</p>")

print("</body>")
print("</html>")
