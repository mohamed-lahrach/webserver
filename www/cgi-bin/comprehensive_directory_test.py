#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Comprehensive CGI Directory Test</title></head>")
print("<body>")
print("<h1>Comprehensive CGI Directory Execution Test</h1>")

# Test 1: Current working directory
current_dir = os.getcwd()
print(f"<p><strong>Current Working Directory:</strong> {current_dir}</p>")

# Test 2: Check if we're in the correct directory
expected_suffix = "www/cgi-bin"
is_correct_dir = current_dir.endswith(expected_suffix)
print(f"<p><strong>Running in correct directory:</strong> {'✅ YES' if is_correct_dir else '❌ NO'}</p>")

# Test 3: Test relative path access to files in same directory
print("<h2>Relative Path Access Tests:</h2>")

# Test reading this script
try:
    with open('./comprehensive_directory_test.py', 'r') as f:
        first_line = f.readline().strip()
    print(f"<p>✅ <strong>Read self with relative path:</strong> {first_line}</p>")
except Exception as e:
    print(f"<p>❌ <strong>Failed to read self:</strong> {e}</p>")

# Test reading another file in the same directory
try:
    with open('./directory_test.py', 'r') as f:
        first_line = f.readline().strip()
    print(f"<p>✅ <strong>Read other script with relative path:</strong> {first_line}</p>")
except Exception as e:
    print(f"<p>❌ <strong>Failed to read other script:</strong> {e}</p>")

# Test 4: Test creating and reading a temporary file
print("<h2>File Creation Test:</h2>")
try:
    with open('./temp_test_file.txt', 'w') as f:
        f.write("This is a test file created by CGI script")
    
    with open('./temp_test_file.txt', 'r') as f:
        content = f.read()
    
    os.remove('./temp_test_file.txt')
    print(f"<p>✅ <strong>Successfully created, read, and deleted temporary file:</strong> {content}</p>")
except Exception as e:
    print(f"<p>❌ <strong>Failed file creation test:</strong> {e}</p>")

# Test 5: List all files in current directory
print("<h2>Directory Contents:</h2>")
try:
    files = sorted(os.listdir('.'))
    print("<ul>")
    for file in files:
        file_path = os.path.join('.', file)
        if os.path.isfile(file_path):
            size = os.path.getsize(file_path)
            print(f"<li>📄 {file} ({size} bytes)</li>")
        elif os.path.isdir(file_path):
            print(f"<li>📁 {file}/</li>")
    print("</ul>")
except Exception as e:
    print(f"<p>❌ <strong>Failed to list directory:</strong> {e}</p>")

# Test 6: Environment variables
print("<h2>CGI Environment Variables:</h2>")
cgi_vars = ['REQUEST_METHOD', 'SCRIPT_NAME', 'SERVER_NAME', 'SERVER_PORT', 'QUERY_STRING', 'GATEWAY_INTERFACE', 'SERVER_PROTOCOL']
print("<ul>")
for var in cgi_vars:
    value = os.environ.get(var, 'Not set')
    print(f"<li><strong>{var}:</strong> {value}</li>")
print("</ul>")

# Test 7: Path information
print("<h2>Path Information:</h2>")
print(f"<p><strong>Script __file__:</strong> {__file__ if '__file__' in globals() else 'Not available'}</p>")
print(f"<p><strong>sys.argv:</strong> {sys.argv}</p>")
print(f"<p><strong>Python executable:</strong> {sys.executable}</p>")

print("</body>")
print("</html>")