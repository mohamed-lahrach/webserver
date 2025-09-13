#!/usr/bin/env python3

import os
import sys

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Enhanced CGI POST Test</title></head>")
print("<body>")
print("<h1>🚀 Enhanced CGI POST Data Analysis</h1>")

# Display request method
method = os.environ.get('REQUEST_METHOD', 'Unknown')
print(f"<p><strong>Request Method:</strong> {method}</p>")

# Display headers information
content_type = os.environ.get('CONTENT_TYPE', 'Not set')
print(f"<p><strong>Content Type:</strong> {content_type}</p>")

# Display file information if available
file_name = os.environ.get('HTTP_X_FILE_NAME', '')
file_type = os.environ.get('HTTP_X_FILE_TYPE', '')
file_size = os.environ.get('HTTP_X_FILE_SIZE', '')

if file_name:
    print(f"<p><strong>📁 File Information:</strong></p>")
    print(f"<ul>")
    print(f"<li><strong>Name:</strong> {file_name}</li>")
    print(f"<li><strong>Type:</strong> {file_type}</li>")
    print(f"<li><strong>Size:</strong> {file_size} bytes</li>")
    print(f"</ul>")

# Try to read from our file
try:
    # Check if we have file info from headers
    file_name = os.environ.get('HTTP_X_FILE_NAME', '')
    if file_name:
        # Trim whitespace from filename
        file_name = file_name.strip()
        file_path = f'/tmp/{file_name}'
        print(f"<p><strong>📁 Looking for file:</strong> {file_path}</p>")
    else:
        file_path = '/tmp/cgi_post_data.txt'
        print(f"<p><strong>📁 Looking for default file:</strong> {file_path}</p>")
    
    with open(file_path, 'rb') as f:  # Read as binary
        file_data = f.read()
    
    print(f"<p><strong>📂 Data from file:</strong></p>")
    print(f"<p><strong>Data length:</strong> {len(file_data)} bytes</p>")
    
    # Determine if it's text or binary
    try:
        # Try to decode as UTF-8
        text_data = file_data.decode('utf-8')
        is_text = True
        
        # Check if it looks like text (printable characters)
        printable_chars = sum(1 for c in text_data if c.isprintable() or c.isspace())
        text_ratio = printable_chars / len(text_data) if text_data else 0
        
        if text_ratio > 0.7:  # If more than 70% printable, treat as text
            print("<p><strong>📝 Content Type:</strong> Text Data</p>")
            print("<pre style='background: #f5f5f5; padding: 10px; border: 1px solid #ddd; max-height: 200px; overflow-y: auto;'>")
            # Limit display to first 1000 characters for readability
            display_text = text_data[:1000]
            if len(text_data) > 1000:
                display_text += "\n... (truncated)"
            print(display_text)
            print("</pre>")
        else:
            is_text = False
    except UnicodeDecodeError:
        is_text = False
    
    if not is_text:
        print("<p><strong>🎭 Content Type:</strong> Binary Data</p>")
        print("<p><strong>Binary Preview (first 100 bytes as hex):</strong></p>")
        print("<pre style='background: #fff8dc; padding: 10px; border: 1px solid #ddd; font-family: monospace;'>")
        hex_data = file_data[:100].hex()
        # Format hex in groups of 16 bytes per line
        for i in range(0, len(hex_data), 32):
            line = hex_data[i:i+32]
            formatted_line = ' '.join(line[j:j+2] for j in range(0, len(line), 2))
            print(formatted_line)
        if len(file_data) > 100:
            print("... (truncated)")
        print("</pre>")
        
        # Try to detect common file types
        if file_data.startswith(b'\xff\xd8\xff'):
            print("<p><strong>🖼️ Detected:</strong> JPEG Image</p>")
        elif file_data.startswith(b'\x89PNG'):
            print("<p><strong>🖼️ Detected:</strong> PNG Image</p>")
        elif file_data.startswith(b'GIF87a') or file_data.startswith(b'GIF89a'):
            print("<p><strong>🖼️ Detected:</strong> GIF Image</p>")
        elif file_data.startswith(b'\x00\x00\x00\x20ftypmp4') or file_data[4:8] == b'ftyp':
            print("<p><strong>🎥 Detected:</strong> MP4 Video</p>")
        elif file_data.startswith(b'RIFF') and b'AVI ' in file_data[:12]:
            print("<p><strong>🎥 Detected:</strong> AVI Video</p>")
        elif file_data.startswith(b'WEBP'):
            print("<p><strong>🖼️ Detected:</strong> WebP Image</p>")
    
except FileNotFoundError:
    print("<p><strong>❌ No POST data file found</strong></p>")
except Exception as e:
    print(f"<p><strong>❌ Error reading file:</strong> {str(e)}</p>")

# Also try reading from stdin (normal CGI way)
content_length = os.environ.get('CONTENT_LENGTH', '0')
print(f"<p><strong>📏 Content Length from environment:</strong> {content_length}</p>")

if method == 'POST' and content_length and content_length != '0':
    try:
        length = int(content_length)
        print(f"<p><strong>📥 Attempting to read {length} bytes from stdin...</strong></p>")
        
        # Read the POST data from stdin (binary mode)
        stdin_data = sys.stdin.buffer.read(length)
        
        print(f"<p><strong>✅ Successfully read {len(stdin_data)} bytes from stdin</strong></p>")
        
        # Quick comparison
        if 'file_data' in locals():
            if stdin_data == file_data:
                print("<p><strong>✅ File data matches stdin data perfectly!</strong></p>")
            else:
                print("<p><strong>⚠️ File data differs from stdin data</strong></p>")
        
    except Exception as e:
        print(f"<p><strong>❌ Error reading from stdin:</strong> {str(e)}</p>")

print("<hr>")
print("<p><strong>🔧 CGI Environment Variables:</strong></p>")
print("<details>")
print("<summary>Click to expand environment details</summary>")
print("<pre style='background: #f0f0f0; padding: 10px; font-size: 12px;'>")
for key, value in sorted(os.environ.items()):
    if key.startswith(('HTTP_', 'CONTENT_', 'REQUEST_', 'SERVER_', 'SCRIPT_')):
        print(f"{key}: {value}")
print("</pre>")
print("</details>")

print("</body>")
print("</html>")
