#!/usr/bin/env python3

import os
import sys
import urllib.parse

def main():
    # Print CGI headers
    print("Content-Type: text/html")
    print()  # Empty line to separate headers from body
    
    # Get request method
    method = os.environ.get('REQUEST_METHOD', 'GET')
    
    print("<!DOCTYPE html>")
    print("<html><head><title>CGI Demo</title></head><body>")
    print("<h1>CGI Script Working!</h1>")
    print(f"<p><strong>Request Method:</strong> {method}</p>")
    
    if method == 'GET':
        query_string = os.environ.get('QUERY_STRING', '')
        print(f"<p><strong>Query String:</strong> {query_string}</p>")
        
        if query_string:
            params = urllib.parse.parse_qs(query_string)
            print("<h2>GET Parameters:</h2><ul>")
            for key, values in params.items():
                for value in values:
                    print(f"<li><strong>{key}:</strong> {value}</li>")
            print("</ul>")
    
    elif method == 'POST':
        content_length = int(os.environ.get('CONTENT_LENGTH', '0'))
        if content_length > 0:
            post_data = sys.stdin.read(content_length)
            print(f"<p><strong>POST Data:</strong> {post_data}</p>")
            
            # Try to parse as form data
            try:
                params = urllib.parse.parse_qs(post_data)
                print("<h2>POST Parameters:</h2><ul>")
                for key, values in params.items():
                    for value in values:
                        print(f"<li><strong>{key}:</strong> {value}</li>")
                print("</ul>")
            except:
                print("<p>Could not parse POST data as form parameters</p>")
    
    # Show environment variables
    print("<h2>Environment Variables:</h2><ul>")
    for key in sorted(os.environ.keys()):
        if key.startswith(('REQUEST_', 'SERVER_', 'GATEWAY_', 'CONTENT_', 'QUERY_', 'SCRIPT_', 'PATH_')):
            print(f"<li><strong>{key}:</strong> {os.environ[key]}</li>")
    print("</ul>")
    
    print("</body></html>")

if __name__ == "__main__":
    main()