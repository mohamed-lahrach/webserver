#!/usr/bin/env python3

import os
import sys

# Read POST data from standard input
post_data = ""
content_length = os.environ.get('CONTENT_LENGTH', '0')

if content_length and content_length.isdigit() and int(content_length) > 0:
    post_data = sys.stdin.read(int(content_length))

# CGI Header
print("Content-Type: text/html")
print()  # Empty line required between headers and content

print("<h1>Hello World from POST!</h1>")
print("<p>Request Method:", os.environ.get('REQUEST_METHOD', 'UNKNOWN'), "</p>")
print("<p>Content Length:", content_length, "</p>")

# Show POST data
if post_data:
    print("<p><strong>Data from standard input:</strong>", post_data, "</p>")
else:
    print("<p>No data received</p>")
    print("<form method='POST' action='simple_post.py'>")
    print("<input type='text' name='message' placeholder='Enter message'>")
    print("<button type='submit'>Send POST</button>")
    print("</form>")
