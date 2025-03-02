FROM gcc:latest

RUN apt-get update && apt-get install -y \
    bash \
    make \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /var/log/erss

WORKDIR /app

COPY . /app

RUN chmod +x /app/run.sh


RUN g++ -static-libstdc++ -std=c++17 -Wall -pthread /app/*.cpp -o /app/my_proxy

RUN chmod +x /app/my_proxy

EXPOSE 12345



# CMD ["tail", "-f", "/dev/null"]
CMD ["/bin/bash", "/app/run.sh"]
