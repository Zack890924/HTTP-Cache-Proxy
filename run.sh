#!/bin/bash
# run.sh: 

echo "==== Starting Proxy Server ===="
./my_proxy &  

echo "==== Wait for proxy to fully start ===="
sleep 5


echo "==== Running BASIC test ===="
bash /app/testcases/test_basic.sh

echo "==== Running CACHE CONTROL test ===="
bash /app/testcases/test_cache_control.sh

echo "==== Running CONNECT test ===="
bash /app/testcases/test_connect.sh

echo "==== Running CHUNKED test ===="
bash /app/testcases/test_chunked.sh

echo "==== Running CONCURRENCY test ===="
bash /app/testcases/test_concurrent.sh

echo "==== All tests completed. Check logs in /var/log/erss or mounted ./logs directory on host ===="


tail -f /dev/null
