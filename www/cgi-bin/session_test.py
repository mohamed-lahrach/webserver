#!/usr/bin/env python3

import os
import uuid

print("Content-Type: text/html")

# Get cookies
http_cookie = os.environ.get('HTTP_COOKIE', '')
session_id = None
visit_count = 0

# Parse existing cookies
if http_cookie:
    for cookie in http_cookie.split(';'):
        if '=' in cookie:
            name, value = cookie.split('=', 1)
            name = name.strip()
            value = value.strip()
            if name == 'SESSION':
                session_id = value
            elif name == 'VISITS':
                visit_count = int(value)

# Create new session if none exists
if not session_id:
    session_id = str(uuid.uuid4())[:8]
    print(f"Set-Cookie: SESSION={session_id}; Path=/; Max-Age=3600")

# Update visit count
visit_count += 1
print(f"Set-Cookie: VISITS={visit_count}; Path=/; Max-Age=3600")

print()

print(f"""<!DOCTYPE html>
<html>
<head><title>Session Test</title></head>
<body style="font-family: Arial; padding: 20px; background: #f5f5f5;">
    <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 400px; margin: 0 auto;">
        <h1>Sessions Work!</h1>
        
        <p><strong>Session ID:</strong> {session_id}</p>
        <p><strong>Visit Count:</strong> {visit_count}</p>
        
        <div style="background: #f8f9fa; padding: 10px; border-radius: 5px; margin-top: 15px;">
            <small><strong>Cookies:</strong> {http_cookie or '(none)'}</small>
        </div>
    </div>
</body>
</html>""")