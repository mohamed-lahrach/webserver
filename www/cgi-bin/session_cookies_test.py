#!/usr/bin/env python3
"""
Simple CGI login example with file-based sessions and cookies.
NOT for production: uses plain-text passwords (demo only).
Improvements to make for production: password hashing (bcrypt), Secure cookie flag,
locking for concurrent file writes, and database-backed sessions.
"""
import os
import sys
import json
import time
import uuid
import html
from urllib.parse import parse_qs

# Configuration
SESSION_DIR = "./sessions"
USERS_FILE = "./users.json"
SESSION_TIMEOUT = 1800  # 30 minutes

# Helpers --------------------------------------------------------------------

def ensure_session_dir():
    if not os.path.isdir(SESSION_DIR):
        os.makedirs(SESSION_DIR, exist_ok=True)

def safe_load_json(path, default):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return default
    except Exception:
        return default

def safe_save_json(path, data):
    # Basic safe write (not atomic). For production consider atomic rename.
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    try:
        os.replace(tmp, path)
    except Exception:
        # Fallback to non-atomic write
        os.remove(tmp)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

# Users ---------------------------------------------------------------------

def load_users():
    """Load users from JSON file or create default users"""
    default_users = {
        "admin": "password123",
        "user": "userpass",
        "test": "test123"
    }
    users = safe_load_json(USERS_FILE, None)
    if isinstance(users, dict):
        return users
    # If file missing or corrupted, write defaults and return them
    safe_save_json(USERS_FILE, default_users)
    return default_users

def save_users(users):
    safe_save_json(USERS_FILE, users)

def authenticate_user(username, password):
    users = load_users()
    return users.get(username) == password

def register_user(username, password):
    users = load_users()
    if username in users:
        return False
    users[username] = password
    save_users(users)
    return True

def user_exists(username):
    users = load_users()
    return username in users

# Session management --------------------------------------------------------

def create_session_id():
    return str(uuid.uuid4())

def _session_path(session_id):
    return os.path.join(SESSION_DIR, f"session_{session_id}.json")

def save_session(session_id, data):
    ensure_session_dir()
    session_file = _session_path(session_id)
    payload = {
        "data": data,
        "timestamp": time.time()
    }
    safe_save_json(session_file, payload)

def load_session(session_id):
    if not session_id:
        return None
    # basic validation to avoid path traversal attacks
    if not all(c.isalnum() or c in "-_" for c in session_id):
        return None
    session_file = _session_path(session_id)
    if not os.path.exists(session_file):
        return None
    try:
        with open(session_file, "r", encoding="utf-8") as f:
            payload = json.load(f)
        ts = payload.get("timestamp", 0)
        if time.time() - ts > SESSION_TIMEOUT:
            try:
                os.remove(session_file)
            except Exception:
                pass
            return None
        return payload.get("data")
    except Exception:
        return None

def delete_session(session_id):
    if not session_id:
        return
    session_file = _session_path(session_id)
    try:
        if os.path.exists(session_file):
            os.remove(session_file)
    except Exception:
        pass

# Cookie parsing ------------------------------------------------------------

def parse_cookies(cookie_header):
    cookies = {}
    if not cookie_header:
        return cookies
    for part in cookie_header.split(";"):
        part = part.strip()
        if not part:
            continue
        if "=" in part:
            k, v = part.split("=", 1)
            cookies[k.strip()] = v.strip()
    return cookies

# HTML rendering helpers ----------------------------------------------------

def esc(s):
    return html.escape(str(s), quote=True)

def render_page(session_data, message, message_type):
    # Validate message_type for CSS class
    if message_type not in ("info", "success", "error"):
        message_type = "info"
    msg_html = ""
    if message:
        msg_html = f'<div class="message {esc(message_type)}">{esc(message)}</div>'

    # Add cookie debug info
    cookie_header = os.environ.get("HTTP_COOKIE", "")
    debug_html = f'''
    <div style="background: #fff3cd; padding: 10px; margin: 10px 0; border-radius: 4px; font-size: 12px;">
        <strong>🐛 Debug Info:</strong><br>
        Incoming Cookies: {esc(cookie_header) if cookie_header else '(none)'}<br>
        Session ID: {esc(session_data.get("session_id", "N/A"))}<br>
        Visit Count: {session_data.get("visit_count", 0)}<br>
        Logged In: {session_data.get("logged_in", False)}
    </div>
    '''

    status_class = "logged-in" if session_data.get("logged_in") else "logged-out"
    status_html = f'<div class="status {status_class}">\n'
    if session_data.get("logged_in"):
        status_html += "<strong>Logged In</strong>"
        status_html += "<br>User: " + esc(session_data.get("username", ""))
    else:
        status_html += "<strong>Not Logged In</strong>"
    status_html += "\n</div>\n"

    demo_info_html = ""
    if not session_data.get("logged_in"):
        demo_info_html = (
            '<div class="demo-info">\n'
            '<h4>Demo Users:</h4>\n'
            'admin / password123<br>\n'
            'user / userpass<br>\n'
            'test / test123\n'
            '</div>\n'
        )

    if session_data.get("logged_in"):
        auth_forms_html = (
            '<div class="text-center mt-15">\n'
            '<form method="POST">\n'
            '<button type="submit" name="logout" value="1" class="btn btn-logout">Logout</button>\n'
            '</form>\n'
            '</div>\n'
        )
    else:
        if session_data.get("show_register"):
            auth_forms_html = (
                '<div>\n'
                '<h3>Create Account</h3>\n'
                '<form method="POST">\n'
                '<div class="form-group">\n'
                '<label for="register_username">Username:</label>\n'
                '<input type="text" id="register_username" name="register_username" required>\n'
                '</div>\n'
                '<div class="form-group">\n'
                '<label for="register_password">Password:</label>\n'
                '<input type="password" id="register_password" name="register_password" required>\n'
                '</div>\n'
                '<div class="form-group">\n'
                '<label for="register_confirm">Confirm Password:</label>\n'
                '<input type="password" id="register_confirm" name="register_confirm" required>\n'
                '</div>\n'
                '<button type="submit" class="btn">Register</button>\n'
                '</form>\n'
                '<div class="text-center mt-15">\n'
                '<form method="POST" style="display: inline;">\n'
                '<button type="submit" name="back_to_login" value="1" class="btn btn-secondary">Back to Login</button>\n'
                '</form>\n'
                '</div>\n'
                '</div>\n'
            )
        else:
            auth_forms_html = (
                '<div>\n'
                '<h3>Login</h3>\n'
                '<form method="POST">\n'
                '<div class="form-group">\n'
                '<label for="username">Username:</label>\n'
                '<input type="text" id="username" name="username" required>\n'
                '</div>\n'
                '<div class="form-group">\n'
                '<label for="password">Password:</label>\n'
                '<input type="password" id="password" name="password" required>\n'
                '</div>\n'
                '<button type="submit" class="btn">Login</button>\n'
                '</form>\n'
                '<div class="text-center mt-15">\n'
                '<form method="POST" style="display: inline;">\n'
                '<button type="submit" name="show_register" value="1" class="btn btn-secondary">Create Account</button>\n'
                '</form>\n'
                '</div>\n'
                '</div>\n'
            )

    full = (
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset='utf-8'>\n"
        "    <title>User Login System</title>\n"
        "    <style>\n"
        "        * {\n"
        "            margin: 0;\n"
        "            padding: 0;\n"
        "            box-sizing: border-box;\n"
        "        }\n"
        "        body {\n"
        "            font-family: Arial, sans-serif;\n"
        "            background: #f5f5f5;\n"
        "            padding: 20px;\n"
        "            min-height: 100vh;\n"
        "        }\n"
        "        .container {\n"
        "            max-width: 400px;\n"
        "            margin: 0 auto;\n"
        "            background: white;\n"
        "            padding: 30px;\n"
        "            border-radius: 8px;\n"
        "            box-shadow: 0 2px 10px rgba(0,0,0,0.1);\n"
        "        }\n"
        "        h1 { text-align: center; color: #333; margin-bottom: 30px; }\n"
        "        .status { padding: 15px; border-radius: 5px; margin-bottom: 20px; text-align: center; }\n"
        "        .logged-in { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }\n"
        "        .logged-out { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }\n"
        "        .form-group { margin-bottom: 15px; }\n"
        "        label { display: block; margin-bottom: 5px; color: #333; font-weight: bold; }\n"
        "        input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 16px; }\n"
        "        input:focus { border-color: #4CAF50; outline: none; }\n"
        "        .btn { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 10px; }\n"
        "        .btn:hover { background: #45a049; }\n"
        "        .btn-secondary { background: #6c757d; width: auto; padding: 8px 16px; margin: 5px; }\n"
        "        .btn-secondary:hover { background: #5a6268; }\n"
        "        .btn-logout { background: #dc3545; }\n"
        "        .btn-logout:hover { background: #c82333; }\n"
        "        .message { padding: 12px; border-radius: 4px; margin-bottom: 20px; text-align: center; }\n"
        "        .success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }\n"
        "        .error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }\n"
        "        .demo-info { background: #e3f2fd; padding: 15px; border-radius: 4px; margin-bottom: 20px; font-size: 14px; }\n"
        "        .demo-info h4 { margin-bottom: 10px; color: #1976d2; }\n"
        "        .text-center { text-align: center; }\n"
        "        .mt-15 { margin-top: 15px; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "<div class='container'>\n"
        "    <h1>User Login System</h1>\n"
        f"{debug_html}\n"
        f"{msg_html}\n"
        f"{status_html}\n"
        f"{demo_info_html}\n"
        f"{auth_forms_html}\n"
        "</div>\n"
        "<script>\n"
        "console.log('🍪 All cookies visible to JavaScript:', document.cookie);\n"
        "console.log('🔄 If you see this in console, JavaScript is working');\n"
        "setTimeout(() => {\n"
        "    const cookieInfo = document.createElement('div');\n"
        "    cookieInfo.style.cssText = 'background:#e7f3ff;padding:10px;margin:10px 0;border-radius:4px;font-size:12px;';\n"
        "    cookieInfo.innerHTML = '<strong>🍪 JS Cookies:</strong> ' + (document.cookie || 'No cookies visible to JavaScript');\n"
        "    document.querySelector('.container').appendChild(cookieInfo);\n"
        "}, 100);\n"
        "</script>\n"
        "</body>\n"
        "</html>\n"
    )
    return full

# Main ----------------------------------------------------------------------

def main():
    ensure_session_dir()

    # Parse cookies and session
    cookie_header = os.environ.get("HTTP_COOKIE", "")
    cookies = parse_cookies(cookie_header)
    session_id = cookies.get("SESSIONID")
    session_data = load_session(session_id) if session_id else None

    if session_data is None:
        session_id = create_session_id()
        session_data = {
            "logged_in": False,
            "username": None,
            "login_time": None,
            "visit_count": 0,
            "show_register": False
        }

    # Read POST data if present
    post_data = {}
    message = ""
    message_type = "info"

    if os.environ.get("REQUEST_METHOD", "").upper() == "POST":
        try:
            content_length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
        except ValueError:
            content_length = 0
        if content_length > 0:
            post_body = sys.stdin.read(content_length)
            parsed = parse_qs(post_body, keep_blank_values=True)
            # Convert single-value lists to strings
            post_data = {k: v[0] if isinstance(v, list) and v else "" for k, v in parsed.items()}

    # Login
    if "username" in post_data and "password" in post_data:
        username = post_data.get("username", "").strip()
        password = post_data.get("password", "")
        if authenticate_user(username, password):
            session_data["logged_in"] = True
            session_data["username"] = username
            session_data["login_time"] = time.strftime("%Y-%m-%d %H:%M:%S")
            session_data["show_register"] = False
            message = f"Welcome {username}! Login successful."
            message_type = "success"
        else:
            message = "Invalid username or password!"
            message_type = "error"

    # Registration
    if "register_username" in post_data and "register_password" in post_data and "register_confirm" in post_data:
        reg_username = post_data.get("register_username", "").strip()
        reg_password = post_data.get("register_password", "")
        reg_confirm = post_data.get("register_confirm", "")
        if not reg_username or not reg_password:
            message = "Username and password are required!"
            message_type = "error"
        elif reg_password != reg_confirm:
            message = "Passwords do not match!"
            message_type = "error"
        elif len(reg_password) < 4:
            message = "Password must be at least 4 characters long!"
            message_type = "error"
        elif user_exists(reg_username):
            message = "Username already exists! Please choose a different one."
            message_type = "error"
        else:
            if register_user(reg_username, reg_password):
                message = f"User '{reg_username}' registered successfully! You can now login."
                message_type = "success"
                session_data["show_register"] = False
            else:
                message = "Registration failed. Please try again."
                message_type = "error"

    # Toggle register form
    if "show_register" in post_data:
        session_data["show_register"] = True
    if "back_to_login" in post_data:
        session_data["show_register"] = False

    # Logout
    if "logout" in post_data:
        delete_session(session_id)
        session_id = create_session_id()
        session_data = {
            "logged_in": False,
            "username": None,
            "login_time": None,
            "visit_count": 0,
            "show_register": False
        }
        message = "Logged out successfully!"
        message_type = "success"

    # Visit count and save session
    try:
        session_data["visit_count"] = int(session_data.get("visit_count", 0)) + 1
    except Exception:
        session_data["visit_count"] = 1

    save_session(session_id, session_data)

    # Store session_id in session_data for debug display
    session_data["session_id"] = session_id

    # Output headers and body
    # For production behind HTTPS: add Secure; and use proper SameSite value as needed
    cookie_header_value = f"SESSIONID={session_id}; Path=/"
    print("Content-Type: text/html; charset=utf-8")
    print(f"Set-Cookie: {cookie_header_value}")
    print()  # End headers

    page = render_page(session_data, message, message_type)
    sys.stdout.write(page)


if __name__ == "__main__":
    main()
