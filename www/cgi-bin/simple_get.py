#!/usr/bin/env python3

import os

# CGI Header
print("Content-Type: text/html")
print()  # Empty line required between headers and content

print("<h1>Hello World from GET!</h1>")
print("<p>Request Method:", os.environ.get('REQUEST_METHOD', 'UNKNOWN'), "</p>")

