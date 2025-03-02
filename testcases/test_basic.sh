
# Basic tests


echo "===== BASIC TESTS ====="

# 1) GET example.com
echo "----- GET http://example.com -----"
curl -x localhost:12345 http://example.com -v > basic_get_1.txt 2>&1

# 2) GET again example.com to test cache
sleep 2
echo "----- GET http://example.com (2nd time, expect 'in cache, valid') -----"
curl -x localhost:12345 http://example.com -v > basic_get_2.txt 2>&1

# 3) test 404
echo "----- GET http://example.com/nonexistentzzzzz (expect 404) -----"
curl -x localhost:12345 http://example.com/nonexistentzzzzz -v > basic_get_404.txt 2>&1

# 4) test POST 
echo "----- POST http://httpbin.org/post -----"
curl -x localhost:12345 -X POST -d "user=abc&pwd=123" http://httpbin.org/post -v > basic_post.txt 2>&1

