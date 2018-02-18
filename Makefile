CC ?= gcc
CFLAGS ?= -m64 -Wall -Wextra -pie -fPIE -fstack-protector-all --param ssp-buffer-size=4 -Wstack-protector -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -O2
LDFLAGS += -Wl,-pie,-z,relro,-z,now `pkg-config --libs libsystemd`

BINDIR = $(DESTDIR)/usr/local/sbin
SYSTEMDDIR = `pkg-config --variable=systemdsystemunitdir systemd`
MANDIR = $(DESTDIR)/usr/local/share/man/man8
NAME = fritzident

all: $(NAME)

$(NAME): debug.o main.o netinfo.o userinfo.o
	$(CC) $(CFLAGS) $(LDFLAGS) debug.o main.o netinfo.o userinfo.o -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

install-man:
	install -d -m644 $(NAME).8 $(MANDIR)/$(NAME).8

install-systemd:
	install -d -m644 $(NAME).service $(SYSTEMDDIR)/$(NAME).service
	install -d -m644 $(NAME).socket $(SYSTEMDDIR)/$(NAME).socket

install-bin:
	install -d $(BINDIR)
	install --mode=755 $(NAME) $(BINDIR)/

install: install-man install-systemd install-bin

clean:
	rm -f *.o $(NAME)

uninstall:
	rm $(BINDIR)/$(NAME)
