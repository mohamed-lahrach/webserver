#!/usr/bin/php
<?php
echo "Content-Type: text/html\r\n\r\n";
echo "<h1>PHP CGI Test</h1>";
echo "<p>Method: " . $_SERVER['REQUEST_METHOD'] . "</p>";
?>