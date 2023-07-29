CC=g++

all:
	$(CC) main.cpp -o espsensorstats

install: all espsensorstats espsensorstats.service
	cp espsensorstats /usr/local/bin
	cp espsensorstats.service /etc/systemd/user
	systemctl daemon-reload

clean: 
	rm espsensorstats

.PHONY: all
