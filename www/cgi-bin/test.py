#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>CGI Test</title></head>")
print("<body>")
print("<h1>CGI Test Script</h1>")
print("<h2>Environment Variables:</h2>")
print("<ul>")

# Print some key CGI environment variables
cgi_vars = ['REQUEST_METHOD', 'QUERY_STRING', 'CONTENT_TYPE', 'CONTENT_LENGTH', 
           'SERVER_NAME', 'SERVER_PORT', 'SCRIPT_NAME', 'HTTP_HOST']

for var in cgi_vars:
    value = os.environ.get(var, 'Not set')
    print(f"<li><strong>{var}:</strong> {value}</li>")

print("</ul>")

# If it's a POST request, read and display the body
if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = os.environ.get('CONTENT_LENGTH')
    if content_length:
        try:
            length = int(content_length)
            post_data = sys.stdin.read(length)
            print("<h2>POST Data:</h2>")
            print(f"<pre>{post_data}</pre>")
        except:
            print("<p>Error reading POST data</p>")

# If there's query string, display it
query_string = os.environ.get('QUERY_STRING', '')
if query_string:
    print("<h2>Query String:</h2>")
    print(f"<p>{query_string}</p>")

print("</body>")
print("</html>")