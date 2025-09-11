#!/usr/bin/env python3

import os
import urllib.parse

print("Content-Type: text/html")
print()

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Cookie Reader</title></head>")
print("<body>")
print("<h1>Cookie Reader</h1>")

# Read cookies from HTTP_COOKIE environment variable
http_cookie = os.environ.get('HTTP_COOKIE', '')

if http_cookie:
    print("<h2>Received Cookies:</h2>")
    print("<table border='1'>")
    print("<tr><th>Cookie Name</th><th>Cookie Value</th></tr>")
    
    # Parse cookies
    cookies = {}
    for cookie in http_cookie.split(';'):
        if '=' in cookie:
            name, value = cookie.strip().split('=', 1)
            cookies[name] = urllib.parse.unquote(value)
            print(f"<tr><td>{name}</td><td>{value}</td></tr>")
    
    print("</table>")
    
    # Check for specific session cookie
    if 'SESSIONID' in cookies:
        print(f"<p><strong>✅ Session Found:</strong> {cookies['SESSIONID']}</p>")
        print("<p>User is logged in with valid session!</p>")
    else:
        print("<p><strong>❌ No Session:</strong> User not logged in</p>")
        
else:
    print("<p><strong>No cookies received</strong></p>")
    print('<p><a href="/cgi-bin/session_test.py">Create Session</a></p>')

# Display all CGI environment for debugging
print("<h2>CGI Environment (Cookie-related):</h2>")
cookie_env_vars = ['HTTP_COOKIE', 'HTTP_HOST', 'REQUEST_METHOD', 'QUERY_STRING']
print("<ul>")
for var in cookie_env_vars:
    value = os.environ.get(var, 'Not set')
    print(f"<li><strong>{var}:</strong> {value}</li>")
print("</ul>")

print("</body>")
print("</html>")
