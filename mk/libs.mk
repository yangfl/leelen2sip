VERSION ?= 0.0
SOVERSION ?= 0

MULTIARCH ?= $(shell $(CC) --print-multiarch)

PREFIX ?= /usr
INCLUDEDIR ?= include
LIBDIR ?= lib/$(MULTIARCH)

LDFLAGS += -Wl,--no-undefined

LIB_NAME := lib$(PROJECT).so
SO_NAME := $(LIB_NAME).$(SOVERSION)
REAL_NAME := $(LIB_NAME).$(VERSION)

ARLIB := lib$(PROJECT).a
SHLIB := $(LIB_NAME)
PCFILE := $(PROJECT).pc

ifeq ($(RELEASE), 0)
	ALL_TARGET := debug
else
	ALL_TARGET := release
endif


.PHONY: all
all: $(ALL_TARGET)

.PHONY: debug
debug: $(SHLIB)

.PHONY: release
release: debug $(ARLIB) $(PCFILE)

$(SO_NAME): $(SHLIB)
	ln -sf $< $@

.PHONY: clean
clean:
	$(RM) $(PREREQUISITES) $(ARLIB) $(SHLIB) $(OBJS) $(PCFILE) $(LIB_NAME)

$(ARLIB): $(OBJS)
	$(AR) rcs $@ $^

$(SHLIB): $(OBJS)
	$(CC) -shared -Wl,-soname,$(SO_NAME) -o $@ $^ $(LDFLAGS)

$(PCFILE): $(PCFILE).in
	sed 's|@prefix@|$(PREFIX)|; s|@libdir@|$(LIBDIR)|; s|@includedir@|$(INCLUDEDIR)|; s|@version@|$(VERSION)|' $< > $@

.PHONY: install-shared
install-shared: $(SHLIB)
	install -d $(DESTDIR)$(PREFIX)/$(LIBDIR) || true
	install -m 0644 $< $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(REAL_NAME)
	rm -f $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(SO_NAME)
	ln -s $(REAL_NAME) $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(SO_NAME)
	rm -f $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(LIB_NAME)
	ln -s $(SO_NAME) $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(LIB_NAME)

.PHONY: install-static
install-static: $(ARLIB)
	install -d $(DESTDIR)$(PREFIX)/$(LIBDIR) || true
	install -m 0644 $< $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(ARLIB)

.PHONY: install-header
install-header: $(HEADERS) $(PCFILE)
	install -d $(DESTDIR)$(PREFIX)/$(INCLUDEDIR) || true
	install -m 0644 $(HEADERS) $(DESTDIR)$(PREFIX)/$(INCLUDEDIR)/
	install -d $(DESTDIR)$(PREFIX)/$(LIBDIR)/pkgconfig || true
	install -m 0644 $(PCFILE) $(DESTDIR)$(PREFIX)/$(LIBDIR)/pkgconfig

.PHONY: install
install: install-static install-header
