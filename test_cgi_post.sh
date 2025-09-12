#!/bin/bash

# CGI POST Test Script
echo "🧪 Testing CGI POST functionality..."
echo "Make sure your webserver is running on port 3080"
echo

SERVER="http://localhost:3080"
CGI_SCRIPT="/cgi-bin/post_test.py"

# Test 1: Simple form data
echo "📝 Test 1: Simple form data"
curl -s -X POST "$SERVER$CGI_SCRIPT" \
     -d "username=testuser&email=test@example.com&message=Hello+World" \
     | grep -E "(POST Data|Content Length|Error)" || echo "❌ Test 1 failed"
echo

# Test 2: JSON data
echo "📝 Test 2: JSON data"
curl -s -X POST "$SERVER$CGI_SCRIPT" \
     -H "Content-Type: application/json" \
     -d '{"name": "John", "age": 30, "city": "New York"}' \
     | grep -E "(POST Data|Content Length|Error)" || echo "❌ Test 2 failed"
echo

# Test 3: Empty POST
echo "📝 Test 3: Empty POST"
curl -s -X POST "$SERVER$CGI_SCRIPT" \
     -d "" \
     | grep -E "(POST Data|Content Length|Error)" || echo "❌ Test 3 failed"
echo

# Test 4: Large data (1KB)
echo "📝 Test 4: Large data (1KB)"
LARGE_DATA=$(python3 -c 'print("x" * 1024)')
curl -s -X POST "$SERVER$CGI_SCRIPT" \
     -d "$LARGE_DATA" \
     | grep -E "(POST Data Length|1024|Error)" || echo "❌ Test 4 failed"
echo

echo "✅ Tests completed. Check the responses above for POST data."
echo "🔍 Also check your server console for CGI debug messages."
