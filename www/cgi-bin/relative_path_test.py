#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Relative Path Access Test</title></head>")
print("<body>")
print("<h1>CGI Relative Path Access Test</h1>")

# Test reading the test data file using relative path
print("<h2>Reading test_data.txt with relative path:</h2>")
try:
    with open('./test_data.txt', 'r') as f:
        content = f.read()
    print("<pre>")
    print(content)
    print("</pre>")
    print("<p><strong>✅ SUCCESS:</strong> Relative path file access works!</p>")
except Exception as e:
    print(f"<p><strong>❌ FAILED:</strong> {e}</p>")

# Test creating a new file using relative path
print("<h2>Creating new file with relative path:</h2>")
try:
    with open('./cgi_created_file.txt', 'w') as f:
        f.write("This file was created by CGI script using relative path.\n")
        f.write(f"Created at: {os.getcwd()}\n")
    print("<p><strong>✅ SUCCESS:</strong> File created with relative path!</p>")
    
    # Verify the file was created
    if os.path.exists('./cgi_created_file.txt'):
        print("<p><strong>✅ VERIFIED:</strong> File exists and is accessible!</p>")
    else:
        print("<p><strong>❌ ERROR:</strong> File was not created!</p>")
        
except Exception as e:
    print(f"<p><strong>❌ FAILED:</strong> {e}</p>")

# Show current working directory
print(f"<p><strong>Current Directory:</strong> {os.getcwd()}</p>")

print("</body>")
print("</html>")