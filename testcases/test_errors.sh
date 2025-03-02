
# test_errors.sh: Test malformed requests or unreachable servers

echo "Running Error Handling Tests..."

echo "Sending malformed request via netcat"
echo -e "GET\r\n\r\n" | nc localhost 12345

echo "Testing an unreachable host"
curl -x localhost:12345 http://thisdomainneverexistzzz123.com -v

# Test partial / invalid HTTP
echo "----- Sending partial HTTP header -----"
echo -e "POST /test HTTP/1.1\r\nHost: google.com\r\nContent-Length:999\r\n\r\n" | nc localhost 12345
