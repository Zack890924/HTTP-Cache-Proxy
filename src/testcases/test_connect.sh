echo "===== CONNECT TEST ====="

# 1) CONNECT www.google.com:443
echo -e "CONNECT www.facebook.com:443 HTTP/1.1\r\nHost: www.facebook.com\r\n\r\n" | nc localhost 12345 > connect_facebook.txt 2>&1
#  "Requesting ..." + "Tunnel closed"

# 2) no complete CONNECT
echo -e "CONNECT \r\n\r\n" | nc localhost 12345 > connect_invalid.txt 2>&1
# "Responding 'HTTP/1.1 400 Bad Request'

sleep 3
