#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>POST Test</title></head>")
print("<body>")
print("<h1>POST Request Test</h1>")

# Display request method
method = os.environ.get('REQUEST_METHOD', 'Unknown')
print(f"<p><strong>Request Method:</strong> {method}</p>")

# Display content length
content_length = os.environ.get('CONTENT_LENGTH', '0')
print(f"<p><strong>Content Length:</strong> {content_length}</p>")

# Display content type
content_type = os.environ.get('CONTENT_TYPE', 'None')
print(f"<p><strong>Content Type:</strong> {content_type}</p>")

# Try to read POST data
if method == 'POST' and content_length and content_length != '0':
    try:
        length = int(content_length)
        print(f"<p><strong>Attempting to read {length} bytes from stdin...</strong></p>")
        
        # Read the POST data
        post_data = sys.stdin.read(length)
        
        print(f"<p><strong>POST Data Length Received:</strong> {len(post_data)} bytes</p>")
        print(f"<p><strong>POST Data:</strong></p>")
        print("<pre style='background: #f5f5f5; padding: 10px; border: 1px solid #ddd;'>")
        print(post_data if post_data else "(empty)")
        print("</pre>")
        
        # Also display raw bytes for debugging
        if post_data:
            print("<p><strong>Raw bytes (first 100):</strong></p>")
            print("<pre style='background: #fff8dc; padding: 10px; border: 1px solid #ddd;'>")
            print(' '.join(f'{ord(c):02x}' for c in post_data[:100]))
            print("</pre>")
        
    except Exception as e:
        print(f"<p><strong>Error reading POST data:</strong> {str(e)}</p>")
else:
    print("<p><strong>No POST data to read</strong></p>")

# Display all environment variables related to CGI
print("<h2>CGI Environment Variables:</h2>")
cgi_vars = ['REQUEST_METHOD', 'CONTENT_TYPE', 'CONTENT_LENGTH', 
           'HTTP_HOST', 'HTTP_USER_AGENT', 'HTTP_COOKIE', 'QUERY_STRING']
print("<table border='1'>")
print("<tr><th>Variable</th><th>Value</th></tr>")
for var in cgi_vars:
    value = os.environ.get(var, 'Not set')
    print(f"<tr><td>{var}</td><td>{value}</td></tr>")
print("</table>")

print("</body>")
print("</html>")
