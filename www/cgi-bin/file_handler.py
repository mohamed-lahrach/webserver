#!/usr/bin/env python3

import os
import sys
import shutil

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>CGI File Handler</title></head>")
print("<body>")
print("<h1>🚀 CGI File Upload Handler</h1>")

# Display request method
method = os.environ.get('REQUEST_METHOD', 'Unknown')
print(f"<p><strong>Request Method:</strong> {method}</p>")

# Display headers information
content_type = os.environ.get('CONTENT_TYPE', 'Not set')
print(f"<p><strong>Content Type:</strong> {content_type}</p>")

# Get file information from headers
file_name = os.environ.get('HTTP_X_FILE_NAME', '').strip()
file_type = os.environ.get('HTTP_X_FILE_TYPE', '').strip()
file_size = os.environ.get('HTTP_X_FILE_SIZE', '').strip()

if file_name:
    print(f"<p><strong>📁 Original File Information:</strong></p>")
    print(f"<ul>")
    print(f"<li><strong>Name:</strong> {file_name}</li>")
    print(f"<li><strong>Type:</strong> {file_type}</li>")
    print(f"<li><strong>Size:</strong> {file_size} bytes</li>")
    print(f"</ul>")

# Content length
content_length = os.environ.get('CONTENT_LENGTH', '0')
print(f"<p><strong>📏 Content Length:</strong> {content_length}</p>")

if method == 'POST':
    try:
        # Read from the temporary file that was saved by the webserver
        temp_file_path = f"/tmp/{file_name}" if file_name else "/tmp/cgi_post_data.txt"
        
        # Check if the file exists
        if os.path.exists(temp_file_path):
            print(f"<p><strong>✅ Found temporary file:</strong> {temp_file_path}</p>")
            
            # Get file stats
            file_stat = os.stat(temp_file_path)
            print(f"<p><strong>📊 File Stats:</strong></p>")
            print(f"<ul>")
            print(f"<li><strong>Size:</strong> {file_stat.st_size} bytes</li>")
            print(f"<li><strong>Last modified:</strong> {file_stat.st_mtime}</li>")
            print(f"</ul>")
            
            if file_name:
                # Copy/move the file to a permanent location with the original name
                upload_dir = "/home/voussama/Desktop/webserver/upload"
                final_path = os.path.join(upload_dir, file_name)
                
                try:
                    # Create upload directory if it doesn't exist
                    os.makedirs(upload_dir, exist_ok=True)
                    
                    # Copy the file to the final location
                    shutil.copy2(temp_file_path, final_path)
                    print(f"<p><strong>✅ File saved to:</strong> {final_path}</p>")
                    
                    # Verify the copy
                    if os.path.exists(final_path):
                        final_stat = os.stat(final_path)
                        print(f"<p><strong>✅ Verification:</strong> File exists, size: {final_stat.st_size} bytes</p>")
                        
                        # Clean up the temporary file
                        os.remove(temp_file_path)
                        print(f"<p><strong>🗑️ Cleaned up temporary file</strong></p>")
                    else:
                        print(f"<p><strong>❌ Error:</strong> File was not copied properly</p>")
                        
                except Exception as e:
                    print(f"<p><strong>❌ Error saving file:</strong> {str(e)}</p>")
            else:
                print(f"<p><strong>📝 Text data saved in:</strong> {temp_file_path}</p>")
                
                # Show preview of text data
                try:
                    with open(temp_file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                        print(f"<p><strong>📄 Content preview:</strong></p>")
                        print("<pre style='background: #f5f5f5; padding: 10px; border: 1px solid #ddd; max-height: 200px; overflow-y: auto;'>")
                        preview = content[:500]  # First 500 characters
                        if len(content) > 500:
                            preview += "\n... (truncated)"
                        print(preview)
                        print("</pre>")
                except UnicodeDecodeError:
                    print(f"<p><strong>🎭 Binary content detected - not showing preview</strong></p>")
        else:
            print(f"<p><strong>❌ Temporary file not found:</strong> {temp_file_path}</p>")
            
        # Also try reading from stdin as backup
        if content_length and content_length != '0':
            try:
                length = int(content_length)
                print(f"<p><strong>📥 Also reading {length} bytes from stdin...</strong></p>")
                stdin_data = sys.stdin.buffer.read(length)
                print(f"<p><strong>✅ Read {len(stdin_data)} bytes from stdin</strong></p>")
            except Exception as e:
                print(f"<p><strong>❌ Error reading stdin:</strong> {str(e)}</p>")
                
    except Exception as e:
        print(f"<p><strong>❌ Error processing request:</strong> {str(e)}</p>")

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
