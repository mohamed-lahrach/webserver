#!/usr/bin/env python3

import os
import sys
import time

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>POST Data Debug</title></head>")
print("<body>")
print("<h1>🔍 POST Data Debug Trace</h1>")

# Get timestamp
timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
print(f"<p><strong>🕐 Timestamp:</strong> {timestamp}</p>")

# Display request method
method = os.environ.get('REQUEST_METHOD', 'Unknown')
print(f"<p><strong>📨 Request Method:</strong> {method}</p>")

# Display content length
content_length = os.environ.get('CONTENT_LENGTH', '0')
print(f"<p><strong>📏 Content Length:</strong> {content_length}</p>")

# Display content type
content_type = os.environ.get('CONTENT_TYPE', 'None')
print(f"<p><strong>📋 Content Type:</strong> {content_type}</p>")

# Display all CGI environment variables
print("<h2>🌍 CGI Environment Variables</h2>")
print("<table border='1' style='border-collapse: collapse; width: 100%;'>")
print("<tr><th>Variable</th><th>Value</th></tr>")
for key, value in sorted(os.environ.items()):
    if key.startswith(('REQUEST_', 'CONTENT_', 'HTTP_', 'SERVER_', 'SCRIPT_', 'GATEWAY_')):
        print(f"<tr><td>{key}</td><td>{value}</td></tr>")
print("</table>")

# Try to read POST data
if method == 'POST' and content_length and content_length != '0':
    try:
        length = int(content_length)
        print(f"<h2>📥 Reading {length} bytes from stdin...</h2>")
        
        # Read the POST data
        print("<p><strong>🔄 Reading data now...</strong></p>")
        post_data = sys.stdin.read(length)
        
        print(f"<h2>✅ POST Data Successfully Received!</h2>")
        print(f"<p><strong>📊 Data Length:</strong> {len(post_data)} bytes</p>")
        print(f"<p><strong>📄 Raw POST Data:</strong></p>")
        print("<div style='background: #f0f0f0; padding: 15px; border: 2px solid #333; margin: 10px 0;'>")
        print(f"<pre>{post_data if post_data else '(empty)'}</pre>")
        print("</div>")
        
        # Parse form data if it looks like form data
        if 'application/x-www-form-urlencoded' in content_type or '=' in post_data:
            print("<h3>🔧 Parsed Form Data:</h3>")
            print("<ul>")
            for pair in post_data.split('&'):
                if '=' in pair:
                    key, value = pair.split('=', 1)
                    # Simple URL decode
                    value = value.replace('+', ' ')
                    print(f"<li><strong>{key}:</strong> {value}</li>")
            print("</ul>")
        
        # Display hex dump for debugging
        if post_data:
            print("<h3>🔢 Hex Dump (first 200 bytes):</h3>")
            print("<pre style='background: #fff8dc; padding: 10px; border: 1px solid #ddd; font-family: monospace;'>")
            hex_data = ' '.join(f'{ord(c):02x}' for c in post_data[:200])
            print(hex_data)
            print("</pre>")
        
        # Save only the raw POST data to temp file
        debug_file = "/tmp/cgi_debug_data.txt"
        try:
            with open(debug_file, 'w') as f:
                f.write(post_data)  # Save only the raw POST data
            print(f"<p><strong>💾 Raw POST data saved to:</strong> {debug_file}</p>")
        except Exception as save_error:
            print(f"<p><strong>⚠️ Could not save to file:</strong> {save_error}</p>")
        
    except Exception as e:
        print(f"<p><strong>❌ Error reading POST data:</strong> {str(e)}</p>")
        print(f"<p><strong>📋 Exception type:</strong> {type(e).__name__}</p>")
else:
    print("<h2>ℹ️ No POST data to read</h2>")
    if method != 'POST':
        print(f"<p>Method is {method}, not POST</p>")
    if not content_length or content_length == '0':
        print(f"<p>Content-Length is {content_length}</p>")

print("<hr>")
print(f"<p><em>Script executed at {timestamp}</em></p>")
print("</body>")
print("</html>")
