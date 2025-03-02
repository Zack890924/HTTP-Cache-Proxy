
echo "===== CONCURRENCY TEST ====="
echo "Sending 10 concurrent requests to example.com..."

for i in {1..10}; do
  curl -x localhost:12345 http://example.com -s -o /dev/null &
done

wait
echo "All concurrent requests finished. Check proxy.log for concurrency issues."
