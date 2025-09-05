#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>POST CGI Directory Test</title></head>")
print("<body>")
print("<h1>POST CGI Directory Test</h1>")

# Test current working directory
current_dir = os.getcwd()
print(f"<p><strong>Current Working Directory:</strong> {current_dir}</p>")

# Check if we're in the correct directory
expected_suffix = "www/cgi-bin"
is_correct_dir = current_dir.endswith(expected_suffix)
print(f"<p><strong>Running in correct directory:</strong> {'✅ YES' if is_correct_dir else '❌ NO'}</p>")

# Read POST data
print("<h2>POST Data Processing:</h2>")
try:
    content_length = int(os.environ.get('CONTENT_LENGTH', '0'))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print(f"<p><strong>✅ Received POST data ({content_length} bytes):</strong> {post_data}</p>")
        
        # Try to save POST data to a file using relative path
        with open('./received_post_data.txt', 'w') as f:
            f.write(post_data)
        
        # Read it back to verify
        with open('./received_post_data.txt', 'r') as f:
            saved_data = f.read()
        
        os.remove('./received_post_data.txt')
        print(f"<p><strong>✅ Successfully saved and read POST data using relative path:</strong> {saved_data}</p>")
    else:
        print("<p><strong>No POST data received</strong></p>")
except Exception as e:
    print(f"<p><strong>❌ Error processing POST data:</strong> {e}</p>")

# Environment variables
print("<h2>Environment Variables:</h2>")
env_vars = ['REQUEST_METHOD', 'CONTENT_TYPE', 'CONTENT_LENGTH', 'SCRIPT_NAME']
print("<ul>")
for var in env_vars:
    value = os.environ.get(var, 'Not set')
    print(f"<li><strong>{var}:</strong> {value}</li>")
print("</ul>")

print("</body>")
print("</html>")