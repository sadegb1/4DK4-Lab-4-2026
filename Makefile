CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS ?= -lm

.PHONY: all test clean

all: arq_sim

SOURCES = main.c packet_arrival.c packet_transmission.c output.c cleanup.c simlib.c
HEADERS = main.h packet_arrival.h packet_transmission.h output.h cleanup.h \
	simparameters.h simlib.h trace.h

arq_sim: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

test: arq_sim
	python3 tests/test_arq.py ./arq_sim

clean:
	rm -f arq_sim
