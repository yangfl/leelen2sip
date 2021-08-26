HOST = 127.1

.PHONY: all
all: discovery

.PHONY: discovery
discovery:
	nc -w 1 -u $(HOST) 6789 < $@.dat
