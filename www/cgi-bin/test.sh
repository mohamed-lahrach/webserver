#!/bin/bash

echo "Content-Type: text/html"
echo ""
echo "<h1>Bash CGI Test</h1>"
echo "<p>Method: $REQUEST_METHOD</p>"
echo "<p>Query: $QUERY_STRING</p>"