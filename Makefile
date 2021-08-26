PROJECT = leelen2sip

LIBS := libosip2

include mk/flags.mk

CFLAGS += -fms-extensions
LDFLAGS += -pthread

SOURCES := $(sort $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c))
OBJS := $(SOURCES:.c=.o)
EXE := $(PROJECT)

.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	$(RM) $(EXE) $(OBJS) $(PREREQUISITES)
	$(RM) -r docs/html

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: doc
doc:
	doxygen

include mk/prerequisties.mk
