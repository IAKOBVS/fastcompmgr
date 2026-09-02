PACKAGES = x11 xcomposite xfixes xdamage xrender
LIBS = `pkg-config --libs ${PACKAGES}` -lm
INCS = `pkg-config --cflags ${PACKAGES}`
CFLAGS ?= -O2 -flto -march=native -pipe
CFLAGS += -Wall -Wextra
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

TESTS = test_timing_buffer test_comp_rect test_win_hash test_pipeline

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

test-integration: fastcompmgr test_helper_win
	@sh test_integration.sh

test_timing_buffer: test_timing_buffer.o cm-util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_timing_buffer.o cm-util.o $(LIBS)

test_timing_buffer.o: test_timing_buffer.c cm-event.c cm-util.h ringbuffer.h
	$(CC) $(CFLAGS) $(INCS) -c test_timing_buffer.c

test_comp_rect: test_comp_rect.o comp_rect.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_comp_rect.o comp_rect.o

test_comp_rect.o: test_comp_rect.c comp_rect.h
	$(CC) $(CFLAGS) -c test_comp_rect.c

test_win_hash: test_win_hash.o cm-util.o comp_rect.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_win_hash.o cm-util.o comp_rect.o $(LIBS)

test_win_hash.o: test_win_hash.c test_support.h cm-window.c cm-event.c cm-util.h ringbuffer.h
	$(CC) $(CFLAGS) $(INCS) -c test_win_hash.c

test_pipeline: test_pipeline.o cm-util.o comp_rect.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_pipeline.o cm-util.o comp_rect.o $(LIBS)

test_pipeline.o: test_pipeline.c test_support.h comp_rect.h cm-window.c cm-event.c cm-util.h ringbuffer.h
	$(CC) $(CFLAGS) $(INCS) -c test_pipeline.c

test_helper_win: test_helper_win.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_helper_win.o $(LIBS)

test_helper_win.o: test_helper_win.c
	$(CC) $(CFLAGS) $(INCS) -c test_helper_win.c

clean:
	rm -f $(OBJS) fastcompmgr $(TESTS) test_helper_win \
		$(TESTS:%=%.o) test_helper_win.o

.PHONY: uninstall clean test test-integration
