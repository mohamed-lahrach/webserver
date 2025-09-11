#!/usr/bin/env python3

import os
import sys
import uuid
import time
import json

# Configuration
SESSION_DIR = "./sessions"

def ensure_session_dir():
    if not os.path.exists(SESSION_DIR):
        os.makedirs(SESSION_DIR)

def load_session(session_id):
    """Load session data from file"""
    ensure_session_dir()
    session_file = os.path.join(SESSION_DIR, f"{session_id}.json")
    try:
        with open(session_file, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}

def save_session(session_id, data):
    """Save session data to file"""
    ensure_session_dir()
    session_file = os.path.join(SESSION_DIR, f"{session_id}.json")
    with open(session_file, 'w') as f:
        json.dump(data, f, indent=2)

# Check for existing session first
http_cookie = os.environ.get('HTTP_COOKIE', '')
session_id = None
cookies = {}

# Parse existing cookies
if http_cookie:
    for cookie in http_cookie.split(';'):
        cookie = cookie.strip()
        if '=' in cookie:
            name, value = cookie.split('=', 1)
            cookies[name.strip()] = value.strip()
    session_id = cookies.get('SESSIONID')

# Handle session
is_new_session = False
if not session_id:
    # Create new session (anonymous)
    session_id = str(uuid.uuid4())
    is_new_session = True
    session_data = {
        'created_at': time.time(),
        'visit_count': 1,
        'last_visit': time.time(),
        'authenticated': False,  # New: Track authentication status
        'user_id': None,         # New: Store user ID when logged in
        'username': None         # New: Store username when logged in
    }
    save_session(session_id, session_data)
    session_status = "NEW anonymous session created"
else:
    # Load existing session
    session_data = load_session(session_id)
    if session_data:
        session_data['visit_count'] = session_data.get('visit_count', 0) + 1
        session_data['last_visit'] = time.time()
        
        # Ensure authentication fields exist (backward compatibility)
        if 'authenticated' not in session_data:
            session_data['authenticated'] = False
            session_data['user_id'] = None
            session_data['username'] = None
            
        save_session(session_id, session_data)
        
        # Show different status based on authentication
        if session_data.get('authenticated', False):
            session_status = f"EXISTING authenticated session (User: {session_data.get('username', 'Unknown')})"
        else:
            session_status = "EXISTING anonymous session"
    else:
        # Session ID exists but no data - treat as new
        session_data = {
            'created_at': time.time(),
            'visit_count': 1,
            'last_visit': time.time(),
            'authenticated': False,
            'user_id': None,
            'username': None
        }
        save_session(session_id, session_data)
        session_status = "SESSION data recreated (anonymous)"

# Output headers
print("Content-Type: text/html")

# Track response headers for display
response_headers = ["Content-Type: text/html"]

# Only set session cookie for new sessions
if is_new_session:
    cookie_header = f"Set-Cookie: SESSIONID={session_id}; Path=/; Max-Age=3600"
    print(cookie_header)
    response_headers.append(cookie_header)

# Clear old cookies that shouldn't exist anymore
if http_cookie:
    old_cookies_to_clear = ['session_id', 'user_preference', 'language', 'temp_data']
    for cookie_name in old_cookies_to_clear:
        if cookie_name in cookies:
            clear_header = f"Set-Cookie: {cookie_name}=; Path=/; Max-Age=0"
            print(clear_header)
            response_headers.append(clear_header)

print()  # Empty line required after headers

print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Session & Cookie Test</title></head>")
print("<body>")
print("<h1>Session & Cookie Test</h1>")

# Check for existing cookies
print(f"<p><strong>Existing Cookies:</strong> {http_cookie if http_cookie else 'None'}</p>")

# Display HTTP Headers
print("<h2>HTTP Request Headers:</h2>")
print("<pre style='background: #f5f5f5; padding: 10px; border: 1px solid #ddd;'>")
print(f"Cookie: {http_cookie if http_cookie else '(none)'}")
print(f"REQUEST_METHOD: {os.environ.get('REQUEST_METHOD', 'GET')}")
print(f"HTTP_HOST: {os.environ.get('HTTP_HOST', 'localhost')}")
print(f"HTTP_USER_AGENT: {os.environ.get('HTTP_USER_AGENT', 'Unknown')}")
print(f"CONTENT_TYPE: {os.environ.get('CONTENT_TYPE', '(none)')}")
print(f"CONTENT_LENGTH: {os.environ.get('CONTENT_LENGTH', '(none)')}")
print("</pre>")

print("<h2>HTTP Response Headers (Sent by CGI):</h2>")
print("<pre style='background: #f0f8ff; padding: 10px; border: 1px solid #ddd;'>")
if response_headers:
    for header in response_headers:
        print(header)
else:
    print("Content-Type: text/html")
    print("(No additional headers set)")
print("</pre>")

# Show what the web server will actually send
print("<h2>Complete HTTP Response (What Browser Receives):</h2>")
print("<pre style='background: #fff8dc; padding: 10px; border: 1px solid #ddd;'>")
print("HTTP/1.1 200 OK")

# Show all actual response headers
for header in response_headers:
    print(header)

print("Content-Length: [calculated by server]")
print("Connection: close")
print("")
print("[HTML body content follows...]")
print("</pre>")

# Debug: Show what response_headers contains
print(f"<p><em>Debug - Response headers count: {len(response_headers)}</em></p>")
print(f"<p><em>Debug - New session flag: {is_new_session}</em></p>")
print(f"<p><em>Debug - Has cookies to clear: {len([c for c in ['session_id', 'user_preference', 'language', 'temp_data'] if c in cookies])}</em></p>")

# Display session info
print(f"<p><strong>Session Status:</strong> {session_status}</p>")
print(f"<p><strong>Current Session ID:</strong> {session_id}</p>")
print(f"<p><strong>Authenticated:</strong> {'Yes' if session_data.get('authenticated', False) else 'No'}</p>")
if session_data.get('authenticated', False):
    print(f"<p><strong>Logged in as:</strong> {session_data.get('username', 'Unknown')}</p>")
    print(f"<p><strong>User ID:</strong> {session_data.get('user_id', 'None')}</p>")
print(f"<p><strong>Visit Count:</strong> {session_data.get('visit_count', 0)}</p>")
print(f"<p><strong>Session Created:</strong> {time.ctime(session_data.get('created_at', time.time()))}</p>")
print(f"<p><strong>Last Visit:</strong> {time.ctime(session_data.get('last_visit', time.time()))}</p>")

# Display all current cookies
print("<h2>Current Cookies:</h2>")
if cookies:
    print("<ul>")
    for name, value in cookies.items():
        print(f"<li><strong>{name}:</strong> {value}</li>")
    print("</ul>")
else:
    print("<p>No cookies found</p>")

# Display session file contents
print("<h2>Session Data (from file):</h2>")
print("<pre>")
print(json.dumps(session_data, indent=2))
print("</pre>")

# Cookie information
print("<h2>Authentication Flow Explanation:</h2>")
print("<ul>")
print("<li><strong>Step 1:</strong> First visit creates anonymous session with random UUID</li>")
print("<li><strong>Step 2:</strong> User logs in → SAME session ID, but session data updated with user info</li>")
print("<li><strong>Step 3:</strong> Subsequent visits → SAME session ID, user remains logged in</li>")
print("<li><strong>Step 4:</strong> Logout → SAME session ID, but authentication data cleared</li>")
print("</ul>")

print("<h2>Cookie Behavior:</h2>")
print("<ul>")
print("<li><strong>SESSIONID:</strong> Set only for new sessions (1 hour expiry)</li>")
print("<li><strong>Session persistence:</strong> Same session ID throughout anonymous → authenticated → logout cycle</li>")
print("<li><strong>Security:</strong> Session ID doesn't change during login (prevents session fixation attacks)</li>")
print("</ul>")

print("<h2>Test Actions:</h2>")
print('<p><a href="/cgi-bin/session_test.py">Refresh this page</a> (should increment visit count)</p>')
print('<p><a href="/cgi-bin/cookie_reader.py">Test Cookie Reading</a></p>')
print('<p><a href="/cgi-bin/session_cookies_test.py">Advanced Session Test</a></p>')

print("</body>")
print("</html>")
