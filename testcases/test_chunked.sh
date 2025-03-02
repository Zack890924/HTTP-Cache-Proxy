#!/bin/bash

echo "===== CHUNKED TEST ====="

curl -x localhost:12345 http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx -v > chunked.txt 2>&1


