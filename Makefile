PACKAGES = x11 xcomposite xfixes xdamage xrender
LIBS = `pkg-config --libs ${PACKAGES}` -lm
INCS = `pkg-config --cflags ${PACKAGES}`
CFLAGS ?= -O2 -flto -pipe
CFLAGS += -Wall -fno-plt
PREFIX = /usr/local
MANDIR = ${PREFIX}/share/man/man1

OBJS=fastcompmgr.o comp_rect.o cm-root.o cm-global.o cm-util.o cm-window.o cm-event.o

.c.o:
	$(CC) $(CFLAGS) $(INCS) -c $*.c

fastcompmgr: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: fastcompmgr
	@mkdir -p "${PREFIX}/bin"
	@cp fastcompmgr "${PREFIX}/bin"
	@mkdir -p "${MANDIR}"
	@cp fastcompmgr.1 "${MANDIR}"

uninstall:
	@rm -f "${PREFIX}/bin/fastcompmgr"
	@rm -f "${MANDIR}/fastcompmgr.1"

test: test_timing_buffer
	./test_timing_buffer

test_timing_buffer: test_timing_buffer.o cm-util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_timing_buffer.o cm-util.o $(LIBS)

test_timing_buffer.o: test_timing_buffer.c cm-event.c cm-util.h ringbuffer.h
	$(CC) $(CFLAGS) $(INCS) -c test_timing_buffer.c

clean:
	rm -f $(OBJS) fastcompmgr test_timing_buffer.o test_timing_buffer

.PHONY: uninstall clean test
