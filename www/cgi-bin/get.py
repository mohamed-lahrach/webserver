#!/usr/bin/env python3

import os

print("Content-Type: text/html")
print()

method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
query_string = os.environ.get('QUERY_STRING', '')

print(f"""<!DOCTYPE html>
<html>
<head><title>GET Test</title></head>
<body>
    <h1>Status: GET Request Successful</h1>
    <p><strong>Method:</strong> {method}</p>
    <p><strong>Query:</strong> {query_string if query_string else '(none)'}</p>
</body>
</html>""")

