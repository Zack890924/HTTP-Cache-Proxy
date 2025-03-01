# 使用 GCC 10，支援 C++17
FROM gcc:10.2

# 安裝必要工具（bash、make）
RUN apt-get update && apt-get install -y \
    bash \
    make \
    && rm -rf /var/lib/apt/lists/*

# 建立日誌目錄
RUN mkdir -p /var/log/erss

# 設定工作目錄
WORKDIR /app

# 複製程式碼與 `run.sh`
COPY src/ /app/src/
COPY run.sh /app/run.sh

# 確保 `run.sh` 可執行
RUN chmod +x /app/run.sh

# 編譯程式，確保輸出到 `/app/my_proxy`
RUN g++ -static-libstdc++ -std=c++17 -Wall -pthread /app/src/*.cpp -o /app/my_proxy

# 確保 `my_proxy` 可執行
RUN chmod +x /app/my_proxy

# 暴露 12345 端口
EXPOSE 12345

# 啟動時執行 `run.sh`
ENTRYPOINT ["bash", "./run.sh"]
