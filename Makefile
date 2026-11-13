CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS ?= -lm

SOURCES = main.c parameters.c simulation.c packet_arrival.c \
	packet_transmission.c channel.c packet_duration.c output.c cleanup.c \
	simlib.c
HEADERS = main.h parameters.h simulation.h packet_arrival.h \
	packet_transmission.h channel.h packet_duration.h output.h cleanup.h \
	simparameters.h simlib.h trace.h

.PHONY: all test clean

all: app

app: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

app_trace: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -DTRACE_ON -o $@ $(SOURCES) $(LDLIBS)

test: app
	python3 tests/test_arq.py ./app

clean:
	rm -f app_trace
