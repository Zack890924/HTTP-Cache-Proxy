#!/bin/bash

PROXY_HOST="localhost"
PROXY_PORT="12345"

# 若你在容器內，且可執行檔是 ./my_proxy，想要在同一腳本中啟動 proxy：
echo "==== 啟動 Proxy 伺服器 ===="

sleep 2   # 等待 Proxy 啟動

echo "==== 開始測試 HTTP Caching Proxy 功能 ===="

# ------------------------------------------------------------------------------
# 1) GET 測試
# ------------------------------------------------------------------------------
echo "----- 測試 GET 請求 -----"
curl -x ${PROXY_HOST}:${PROXY_PORT} http://example.com -v > test_get_output.txt 2>&1
echo "GET 請求完成，結果存放在 test_get_output.txt"
echo

# ------------------------------------------------------------------------------
# 2) POST 測試
# ------------------------------------------------------------------------------
echo "----- 測試 POST 請求 -----"
curl -x ${PROXY_HOST}:${PROXY_PORT} -X POST http://httpbin.org/post -d "name=test&value=123" -v > test_post_output.txt 2>&1
echo "POST 請求完成，結果存放在 test_post_output.txt"
echo

# ------------------------------------------------------------------------------
# 3) CONNECT 測試
# ------------------------------------------------------------------------------
echo "----- 測試 CONNECT 隧道 -----"
echo -e "CONNECT www.google.com:443 HTTP/1.1\r\nHost: www.google.com\r\n\r\n" \
  | nc -v -w 5 ${PROXY_HOST} ${PROXY_PORT} > test_connect_output.txt 2>&1
echo "CONNECT 請求完成，結果存放在 test_connect_output.txt"
echo

# ------------------------------------------------------------------------------
# 4) 快取測試 (連續兩次 GET)
# ------------------------------------------------------------------------------
echo "----- 測試快取行為(連續兩次 GET) -----"
curl -x ${PROXY_HOST}:${PROXY_PORT} http://example.com -v > test_cache_first.txt 2>&1
sleep 2
curl -x ${PROXY_HOST}:${PROXY_PORT} http://example.com -v > test_cache_second.txt 2>&1
echo "兩次 GET 請求已完成，請查看 test_cache_first.txt 與 test_cache_second.txt 以確認快取狀態"
echo

# ------------------------------------------------------------------------------
# 5) 測試 "過期" 或 "must-revalidate" (可選)
# ------------------------------------------------------------------------------
# 如果你能找到一個 max-age 很短(如5秒)的網站，或者你有個自建server，
# 可以再做一下「先GET -> 等待超過max-age -> 再GET」的測試

SHORT_MAX_AGE_URL="http://example.com"  # 改成你確定帶短 max-age 的URL
echo "----- 測試 過期/必須重新驗證 (可選) -----"
echo " (可能需要你確保該站點返回 Cache-Control: max-age=5 或 must-revalidate)"
curl -x ${PROXY_HOST}:${PROXY_PORT} $SHORT_MAX_AGE_URL -v > test_expire_first.txt 2>&1
echo "已取得一次 GET，等待 10 秒以確定它過期..."
sleep 10
curl -x ${PROXY_HOST}:${PROXY_PORT} $SHORT_MAX_AGE_URL -v > test_expire_second.txt 2>&1
echo "過期後再次GET，請查看 test_expire_first.txt / test_expire_second.txt"
echo

# ------------------------------------------------------------------------------
# 6) 測試 400 Bad Request (使用 netcat發送錯誤的 request line)
# ------------------------------------------------------------------------------
echo "----- 測試 400 Bad Request -----"
echo -e "GARBAGEREQUEST\n\n" \
  | nc -v -w 5 ${PROXY_HOST} ${PROXY_PORT} > test_400_output.txt 2>&1
echo "若 Proxy 回傳 400，結果存放在 test_400_output.txt"
echo

# ------------------------------------------------------------------------------
# 7) 測試 502 Bad Gateway
# ------------------------------------------------------------------------------
# 例如連到一個會返回破損 response 的 netcat server, 這裡示範指令
# 你需要先在另一個shell運行 "nc -l 9999" 然後手動打不完整HTTP回應 => 觀察Proxy
# 不過這裡給個參考而已:
: '
echo "----- 測試 502 Bad Gateway (需自備破損response server) -----"
curl -x ${PROXY_HOST}:${PROXY_PORT} http://127.0.0.1:9999 -v > test_502_output.txt 2>&1
echo "結果存放在 test_502_output.txt (若 server 給破損回應, Proxy應該回 502)"
echo
'

# ------------------------------------------------------------------------------
# 8) 多並發測試
# ------------------------------------------------------------------------------
echo "----- 測試多並發 -----"
# 一口氣執行 10 條 GET
for i in {1..10}; do
    curl -x ${PROXY_HOST}:${PROXY_PORT} http://example.com -s -o /dev/null &
done
wait
echo "已完成 10 個並發GET測試(請查看 proxy.log 有無多個ID同時出現, Proxy有無崩潰)"

# ------------------------------------------------------------------------------
echo "所有測試完成！"
echo "請查看產生的 test_*.txt 檔案及 logs/proxy.log 確認行為正確。"

# ------------------------------------------------------------------------------
# 讓容器保持運行(若你在Docker容器內要持續不退出):
tail -f /dev/null
