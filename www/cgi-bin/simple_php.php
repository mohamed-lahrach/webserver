#!/usr/bin/php
<?php
echo "Content-Type: text/html\r\n\r\n";
if ($_SERVER['REQUEST_METHOD'] == 'GET') echo "PHP GET works!";
else echo "PHP POST works!";
