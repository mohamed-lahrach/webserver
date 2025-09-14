#!/usr/bin/env python3

import os
import sys
import uuid
import json

# Simple session test - just creates/shows a session
print("Content-Type: text/html")

# Check if we have an existing session cookie
http_cookie = os.environ.get('HTTP_COOKIE', '')
session_id = None

if http_cookie:
    for cookie in http_cookie.split(';'):
        cookie = cookie.strip()
        if '=' in cookie:
            name, value = cookie.split('=', 1)
            if name.strip() == 'SESSION':
                session_id = value.strip()

# If no session, create one
if not session_id:
    session_id = str(uuid.uuid4())[:8]  # Short session ID for testing
    print(f"Set-Cookie: SESSION={session_id}; Path=/; Max-Age=3600")

print()  # Empty line after headers

# Simple HTML response
print(f"""<!DOCTYPE html>
<html>
<head>
    <title>Simple Session Test</title>
    <style>
        body {{ font-family: Arial, sans-serif; padding: 20px; background: #f0f8ff; }}
        .session-box {{ 
            background: white; 
            padding: 20px; 
            border-radius: 10px; 
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            max-width: 600px;
            margin: 0 auto;
        }}
        .success {{ color: #28a745; font-weight: bold; }}
        .info {{ color: #17a2b8; }}
    </style>
</head>
<body>
    <div class="session-box">
        <h1>🍪 Simple Session Test</h1>
        
        <div class="info">
            <h3>Session Information:</h3>
            <p><strong>Session ID:</strong> {session_id}</p>
            <p><strong>Cookie Name:</strong> SESSION</p>
            <p><strong>Status:</strong> <span class="success">Session Active</span></p>
        </div>
        
        <div>
            <h3>What happened:</h3>
            <ul>
                <li>Server checked for existing session cookie</li>
                <li>{'Found existing session!' if http_cookie and 'SESSION' in http_cookie else 'Created new session cookie'}</li>
                <li>Session will persist for 1 hour</li>
            </ul>
        </div>
        
        <div>
            <h3>Test Actions:</h3>
            <p><a href="/cgi-bin/simple_session_test.py">🔄 Refresh</a> (should keep same session ID)</p>
            <p><a href="/main_test.html">← Back to Main Test</a></p>
        </div>
        
        <div style="background: #f8f9fa; padding: 15px; border-radius: 5px; margin-top: 20px;">
            <h4>Debug Info:</h4>
            <p><strong>HTTP_COOKIE:</strong> {http_cookie if http_cookie else '(none)'}</p>
            <p><strong>REQUEST_METHOD:</strong> {os.environ.get('REQUEST_METHOD', 'GET')}</p>
        </div>
    </div>
</body>
</html>""")
