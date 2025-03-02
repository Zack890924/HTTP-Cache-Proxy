

echo "===== CACHE CONTROL TESTS ====="

# no-store
echo "----- GET http://www.mocky.io/v2/5e58a7d92f0000851296205a (no-store) -----"
curl -x localhost:12345 http://www.mocky.io/v2/5e58a7d92f0000851296205a -v > no_store.txt 2>&1
#"not cacheable because no-store"

# max-age=10
echo "----- GET mocky with max-age=5 -----"
curl -x localhost:12345 http://www.mocky.io/v2/5e5892de2f00006d0b961fdc -v > maxage_1.txt 2>&1
sleep 3
curl -x localhost:12345 http://www.mocky.io/v2/5e5892de2f00006d0b961fdc -v > maxage_2.txt 2>&1
sleep 12
curl -x localhost:12345 http://www.mocky.io/v2/5e5892de2f00006d0b961fdc -v > maxage_3.txt 2>&1
# 1. not in cache, 2. in cache, valid 3. expired

# Expires
echo "----- GET mocky with Expires -----"
curl -x localhost:12345 http://www.mocky.io/v2/5e589a932f0000bd0c962006 -v > expires_1.txt 2>&1
sleep 2
curl -x localhost:12345 http://www.mocky.io/v2/5e589a932f0000bd0c962006 -v > expires_2.txt 2>&1
# "cached, expires at ..."

