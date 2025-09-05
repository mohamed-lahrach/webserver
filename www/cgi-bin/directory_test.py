#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>CGI Directory Test</title></head>")
print("<body>")
print("<h1>CGI Directory Execution Test</h1>")

# Test 1: Current working directory
print(f"<p><strong>Current Working Directory:</strong> {os.getcwd()}</p>")

# Test 2: Script location
script_path = os.path.abspath(__file__) if '__file__' in globals() else 'Not available'
print(f"<p><strong>Script Absolute Path:</strong> {script_path}</p>")

# Test 3: Expected directory (should be www/cgi-bin)
expected_dir = "www/cgi-bin"
current_dir = os.getcwd()
is_correct_dir = current_dir.endswith(expected_dir)
print(f"<p><strong>Running in correct directory:</strong> {'✅ YES' if is_correct_dir else '❌ NO'}</p>")

# Test 4: List files in current directory (relative path access)
print("<h2>Files in Current Directory (relative path test):</h2>")
try:
    files = os.listdir('.')
    print("<ul>")
    for file in sorted(files):
        print(f"<li>{file}</li>")
    print("</ul>")
except Exception as e:
    print(f"<p><strong>❌ Error accessing current directory:</strong> {e}</p>")

# Test 5: Try to read a file using relative path
print("<h2>Relative File Access Test:</h2>")
try:
    # Try to read this script using relative path
    with open('./directory_test.py', 'r') as f:
        first_line = f.readline().strip()
    print(f"<p><strong>✅ Successfully read file with relative path:</strong> {first_line}</p>")
except Exception as e:
    print(f"<p><strong>❌ Failed to read file with relative path:</strong> {e}</p>")

# Test 6: Environment variables
print("<h2>CGI Environment Variables:</h2>")
cgi_vars = ['REQUEST_METHOD', 'SCRIPT_NAME', 'SERVER_NAME', 'SERVER_PORT', 'QUERY_STRING']
print("<ul>")
for var in cgi_vars:
    value = os.environ.get(var, 'Not set')
    print(f"<li><strong>{var}:</strong> {value}</li>")
print("</ul>")

print("</body>")
print("</html>")