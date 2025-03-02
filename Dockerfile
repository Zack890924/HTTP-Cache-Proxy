
FROM gcc:10.2


RUN apt-get update && apt-get install -y \
    bash \
    make \
    && rm -rf /var/lib/apt/lists/*


RUN mkdir -p /var/log/erss


WORKDIR /app


COPY src/ /app/src/
COPY run.sh /app/run.sh
COPY testcases/ /app/testcases/

RUN chmod +x /app/run.sh


RUN g++ -static-libstdc++ -std=c++17 -Wall -pthread /app/src/*.cpp -o /app/my_proxy


RUN chmod +x /app/my_proxy


EXPOSE 12345


# ENTRYPOINT ["bash", "./run.sh"]
CMD ["tail", "-f", "/dev/null"]
