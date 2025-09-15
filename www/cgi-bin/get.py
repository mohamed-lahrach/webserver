#!/usr/bin/env python3

import os

print("Content-Type: text/html")
print()

print(f"""<!DOCTYPE html>
<html>
<head><title>GET Test</title></head>
<body>
    <h1>✅ GET Request Works!</h1>
    <p><strong>Method:</strong> {os.environ.get('REQUEST_METHOD', 'UNKNOWN')}</p>
    <p><strong>Status:</strong> CGI execution successful</p>
</body>
</html>""")

