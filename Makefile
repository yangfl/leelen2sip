PROJECT = leelen2sip

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	RELEASE ?= 0
else
	RELEASE ?= 1
endif

# default
CPPFLAGS ?= -fdiagnostics-color=always
ifeq ($(RELEASE), 0)
	CFLAGS ?= -Os -fPIC
else
	CFLAGS ?= -O2 -fPIC
endif
LDFLAGS ?= -pie

# debug
ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif
ifeq ($(RELEASE), 0)
	CPPFLAGS += -DDEBUG
endif

# hardening
CPPFLAGS += -D_FORTIFY_SOURCE=2
CFLAGS += -fstack-protector-strong
LDFLAGS += -Wl,-z,relro

# diagnosis
CWARN ?= -Wall -Wextra -Wpedantic -Werror=format-security \
	-Wno-cast-function-type -Wno-missing-field-initializers
ifeq ($(DEBUG), 1)
	CWARN += -fanalyzer
endif
CFLAGS += $(CWARN)

# program flags
CPPFLAGS += -D_DEFAULT_SOURCE -I.
CFLAGS += -std=c2x -fms-extensions
LDFLAGS += -Wl,--gc-sections -Wl,--as-needed -Wl,-z,defs -flto=auto -fwhole-program -pthread

LIBS := libosip2
PKG_CONFIG ?= pkg-config
LIBS_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags-only-I $(LIBS))
LIBS_CFLAGS := $(shell $(PKG_CONFIG) --cflags-only-other $(LIBS))
LIBS_LDFLAGS := $(shell $(PKG_CONFIG) --libs $(LIBS))

CPPFLAGS += $(LIBS_CPPFLAGS)
CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)

SOURCES := $(wildcard *.c) $(wildcard */*.c)
OBJS := $(sort $(SOURCES:.c=.o))
PREREQUISITES := $(SOURCES:.c=.d)
THIS_MAKEFILE_LIST := $(MAKEFILE_LIST)

EXE := $(PROJECT)


.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	$(RM) $(EXE) $(OBJS) $(PREREQUISITES)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< | sed 's,.*\.o *:,$(<:.c=.o) $@: $(THIS_MAKEFILE_LIST),' > $@

-include $(PREREQUISITES)

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
