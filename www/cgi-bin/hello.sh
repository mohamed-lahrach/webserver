#!/bin/bash
echo "Content-Type: text/html"
echo ""
echo "<h1>Hello World from Bash!</h1>"
if [ "$REQUEST_METHOD" = "GET" ]; then
    echo "<p>Bash GET request works!</p>"
    echo "<p>Query: $QUERY_STRING</p>"
else
    echo "<p>Bash POST request works!</p>"
fi
echo "<p>Server: $(uname -s) $(uname -r)</p>"
