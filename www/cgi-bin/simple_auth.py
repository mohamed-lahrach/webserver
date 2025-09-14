#!/usr/bin/env python3

import os
import sys
import json
import uuid
import time
import hashlib
from urllib.parse import parse_qs

# Simple user database (in real app, use proper database)
USERS_FILE = "./users.json"
SESSION_DIR = "./sessions"

def ensure_directories():
    """Create necessary directories"""
    if not os.path.exists(SESSION_DIR):
        os.makedirs(SESSION_DIR)
    
    # Create users file if it doesn't exist
    if not os.path.exists(USERS_FILE):
        with open(USERS_FILE, 'w') as f:
            json.dump({}, f)

def load_users():
    """Load users from file"""
    ensure_directories()
    try:
        with open(USERS_FILE, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}

def save_users(users):
    """Save users to file"""
    ensure_directories()
    with open(USERS_FILE, 'w') as f:
        json.dump(users, f, indent=2)

def hash_password(password):
    """Simple password hashing"""
    return hashlib.sha256(password.encode()).hexdigest()

def create_user(username, password):
    """Create a new user"""
    users = load_users()
    if username in users:
        return False, "User already exists"
    
    users[username] = {
        'password': hash_password(password),
        'created_at': time.time(),
        'user_id': str(uuid.uuid4())
    }
    save_users(users)
    return True, "User created successfully"

def authenticate_user(username, password):
    """Authenticate user"""
    users = load_users()
    if username not in users:
        return False, "User not found"
    
    if users[username]['password'] != hash_password(password):
        return False, "Invalid password"
    
    return True, users[username]

def load_session(session_id):
    """Load session data"""
    if not session_id:
        return {}
    
    session_file = os.path.join(SESSION_DIR, f"{session_id}.json")
    try:
        with open(session_file, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}

def save_session(session_id, data):
    """Save session data"""
    ensure_directories()
    session_file = os.path.join(SESSION_DIR, f"{session_id}.json")
    with open(session_file, 'w') as f:
        json.dump(data, f, indent=2)

def get_session_id():
    """Get session ID from cookies"""
    http_cookie = os.environ.get('HTTP_COOKIE', '')
    if http_cookie:
        for cookie in http_cookie.split(';'):
            cookie = cookie.strip()
            if '=' in cookie:
                name, value = cookie.split('=', 1)
                if name.strip() == 'SESSIONID':
                    return value.strip()
    return None

def create_session(authenticated=False, user_data=None):
    """Create new session"""
    session_id = str(uuid.uuid4())
    session_data = {
        'created_at': time.time(),
        'last_visit': time.time(),
        'authenticated': authenticated
    }
    
    if authenticated and user_data:
        session_data['username'] = user_data.get('username')
        session_data['user_id'] = user_data.get('user_id')
    
    save_session(session_id, session_data)
    return session_id, session_data

def main():
    # Get request method and form data
    method = os.environ.get('REQUEST_METHOD', 'GET')
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    
    form_data = {}
    action = os.environ.get('QUERY_STRING', '')
    
    if method == 'POST' and content_length > 0:
        post_data = sys.stdin.read(content_length)
        form_data = parse_qs(post_data)
        # Convert lists to single values
        for key, value in form_data.items():
            if isinstance(value, list) and len(value) > 0:
                form_data[key] = value[0]
    
    # Get current session
    session_id = get_session_id()
    session_data = load_session(session_id) if session_id else {}
    
    # Handle actions
    response_message = ""
    new_session_created = False
    
    if 'action' in form_data:
        action = form_data['action']
        
        if action == 'register' and 'username' in form_data and 'password' in form_data:
            success, message = create_user(form_data['username'], form_data['password'])
            response_message = f"Registration: {message}"
            
        elif action == 'login' and 'username' in form_data and 'password' in form_data:
            success, result = authenticate_user(form_data['username'], form_data['password'])
            if success:
                # Create or update session with user data
                if not session_id:
                    session_id, session_data = create_session(True, {
                        'username': form_data['username'],
                        'user_id': result['user_id']
                    })
                    new_session_created = True
                else:
                    # Update existing session
                    session_data['authenticated'] = True
                    session_data['username'] = form_data['username']
                    session_data['user_id'] = result['user_id']
                    session_data['last_visit'] = time.time()
                    save_session(session_id, session_data)
                
                response_message = f"Login successful! Welcome {form_data['username']}"
            else:
                response_message = f"Login failed: {result}"
                
        elif action == 'logout':
            if session_id and session_data:
                # Clear authentication but keep session
                session_data['authenticated'] = False
                session_data['username'] = None
                session_data['user_id'] = None
                session_data['last_visit'] = time.time()
                save_session(session_id, session_data)
                response_message = "Logged out successfully"
            else:
                response_message = "No active session to logout"
    
    # If no session exists, create anonymous one
    if not session_id:
        session_id, session_data = create_session()
        new_session_created = True
    
    # Output headers
    print("Content-Type: text/html")
    if new_session_created:
        print(f"Set-Cookie: SESSIONID={session_id}; Path=/; Max-Age=3600")
    print()  # Empty line after headers
    
    # HTML output
    print("""<!DOCTYPE html>
<html>
<head>
    <title>Simple Authentication Test</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
        .form-section { background: #f5f5f5; padding: 20px; margin: 20px 0; border-radius: 5px; }
        .status { background: #e8f5e8; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .error { background: #ffe8e8; }
        input, button { padding: 8px; margin: 5px; }
        button { background: #4CAF50; color: white; border: none; border-radius: 3px; cursor: pointer; }
        button:hover { background: #45a049; }
        .logout-btn { background: #f44336; }
        .logout-btn:hover { background: #da190b; }
    </style>
</head>
<body>
    <h1>Simple Authentication Test</h1>""")
    
    # Show response message
    if response_message:
        status_class = "error" if "failed" in response_message.lower() else ""
        print(f'<div class="status {status_class}"><strong>{response_message}</strong></div>')
    
    # Show current session status
    print('<div class="status">')
    if session_data.get('authenticated'):
        print(f'<strong>Status:</strong> Logged in as <strong>{session_data.get("username")}</strong>')
        print(f'<br><strong>Session ID:</strong> {session_id}')
        print(f'<br><strong>User ID:</strong> {session_data.get("user_id")}')
    else:
        print('<strong>Status:</strong> Not logged in (anonymous session)')
        print(f'<br><strong>Session ID:</strong> {session_id}')
    print('</div>')
    
    # Show forms based on authentication status
    if not session_data.get('authenticated'):
        # Registration form
        print("""
    <div class="form-section">
        <h2>Create Account</h2>
        <form method="post">
            <input type="hidden" name="action" value="register">
            <input type="text" name="username" placeholder="Username" required>
            <input type="password" name="password" placeholder="Password" required>
            <button type="submit">Create Account</button>
        </form>
    </div>
    
    <div class="form-section">
        <h2>Login</h2>
        <form method="post">
            <input type="hidden" name="action" value="login">
            <input type="text" name="username" placeholder="Username" required>
            <input type="password" name="password" placeholder="Password" required>
            <button type="submit">Login</button>
        </form>
    </div>""")
    else:
        # Logout form
        print("""
    <div class="form-section">
        <h2>Account Actions</h2>
        <form method="post">
            <input type="hidden" name="action" value="logout">
            <button type="submit" class="logout-btn">Logout</button>
        </form>
    </div>""")
    
    # Show session information
    print(f"""
    <div class="form-section">
        <h2>Session Information</h2>
        <p><strong>Session ID:</strong> {session_id}</p>
        <p><strong>Authenticated:</strong> {'Yes' if session_data.get('authenticated') else 'No'}</p>
        <p><strong>Created:</strong> {time.ctime(session_data.get('created_at', time.time()))}</p>
        <p><strong>Last Visit:</strong> {time.ctime(session_data.get('last_visit', time.time()))}</p>
    </div>
    
    <div class="form-section">
        <h2>Test Links</h2>
        <p><a href="/cgi-bin/simple_auth.py">Refresh Page</a> (should stay logged in)</p>
        <p><a href="/main_test.html">Back to Main Test Page</a></p>
    </div>
    
</body>
</html>""")

if __name__ == "__main__":
    main()
