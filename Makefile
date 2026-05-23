CC       = gcc
CFLAGS   = -O2
PKGFLAGS = $(shell pkg-config --cflags --libs gtk+-3.0 vte-2.91)
LIBS     = -lX11 -lm

BINARY   = liquid_glass_gtk
VERSION  = 1.0.0
ARCH     = amd64
DEB      = liquid-glass_$(VERSION)_$(ARCH).deb
DEB_DIR  = .debpkg

PREFIX   = /usr
BINDIR   = $(PREFIX)/bin
ICONDIR  = $(PREFIX)/share/icons/hicolor/256x256/apps
APPDIR   = $(PREFIX)/share/applications
DOCDIR   = $(PREFIX)/share/doc/liquid-glass

.PHONY: all binary deb clean install uninstall

all: binary

liquid_icon_data.h: liquid.png
	xxd -i liquid.png > liquid_icon_data.h

binary: liquid_icon_data.h liquid_glass_gtk.c
	$(CC) $(CFLAGS) -DEMBED_ICON -o $(BINARY) liquid_glass_gtk.c $(PKGFLAGS) $(LIBS)

deb: binary
	rm -rf $(DEB_DIR)
	mkdir -p $(DEB_DIR)/DEBIAN
	mkdir -p $(DEB_DIR)/$(BINDIR)
	mkdir -p $(DEB_DIR)/$(ICONDIR)
	mkdir -p $(DEB_DIR)/$(APPDIR)
	mkdir -p $(DEB_DIR)/$(DOCDIR)

	cp $(BINARY)    $(DEB_DIR)/$(BINDIR)/$(BINARY)
	cp liquid.png   $(DEB_DIR)/$(ICONDIR)/liquid_glass.png

	printf '[Desktop Entry]\nName=Liquid Glass\nComment=Liquid Glass Terminal\n' > $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop
	printf 'Exec=$(BINDIR)/$(BINARY)\nIcon=liquid_glass\nTerminal=false\n'      >> $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop
	printf 'Type=Application\nCategories=System;TerminalEmulator;\n'            >> $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop
	printf 'StartupWMClass=$(BINARY)\n'                                         >> $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop

	printf 'Liquid Glass Terminal\nCopyright (C) 2026 Chokri Hammedi\nAll rights reserved.\n' \
	    > $(DEB_DIR)/$(DOCDIR)/copyright

	printf 'Package: liquid-glass\nVersion: $(VERSION)\nArchitecture: $(ARCH)\n'       > $(DEB_DIR)/DEBIAN/control
	printf 'Maintainer: Chokri Hammedi <chokrihammedi51@gmail.com>\n'                  >> $(DEB_DIR)/DEBIAN/control
	printf 'Depends: libgtk-3-0, libvte-2.91-0, libx11-6\n'                           >> $(DEB_DIR)/DEBIAN/control
	printf 'Section: x11\nPriority: optional\n'                                        >> $(DEB_DIR)/DEBIAN/control
	printf 'Homepage: https://github.com/blue0x1/liquid-glass\n'                       >> $(DEB_DIR)/DEBIAN/control
	printf 'Description: Liquid Glass Terminal\n'                                      >> $(DEB_DIR)/DEBIAN/control
	printf ' A GTK3 terminal emulator with a liquid glass aesthetic.\n'                >> $(DEB_DIR)/DEBIAN/control
	printf ' Features translucent frosted-glass panels, KWin blur-behind\n'            >> $(DEB_DIR)/DEBIAN/control
	printf ' support, tabbed sessions, a collapsible sidebar, and a live\n'            >> $(DEB_DIR)/DEBIAN/control
	printf ' theme/opacity settings window.\n'                                         >> $(DEB_DIR)/DEBIAN/control

	printf '#!/bin/sh\nset -e\n'                                                        > $(DEB_DIR)/DEBIAN/postinst
	printf 'gtk-update-icon-cache -f -t /usr/share/icons/hicolor/ 2>/dev/null || true\n' >> $(DEB_DIR)/DEBIAN/postinst
	printf 'update-desktop-database /usr/share/applications 2>/dev/null || true\n'    >> $(DEB_DIR)/DEBIAN/postinst

	chmod 755 $(DEB_DIR)/DEBIAN/postinst
	chmod 755 $(DEB_DIR)/$(BINDIR)/$(BINARY)
	dpkg-deb --root-owner-group --build $(DEB_DIR) $(DEB)
	rm -rf $(DEB_DIR)
	@echo ""
	@echo "Built: $(DEB)"

install: binary
	install -Dm755 $(BINARY)   $(DESTDIR)$(BINDIR)/$(BINARY)
	install -Dm644 liquid.png  $(DESTDIR)$(ICONDIR)/liquid_glass.png
	install -Dm644 liquid_glass.desktop $(DESTDIR)$(APPDIR)/liquid_glass.desktop
	gtk-update-icon-cache -f -t $(DESTDIR)$(PREFIX)/share/icons/hicolor/ 2>/dev/null || true
	update-desktop-database $(DESTDIR)$(APPDIR) 2>/dev/null || true

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BINARY)
	rm -f $(DESTDIR)$(ICONDIR)/liquid_glass.png
	rm -f $(DESTDIR)$(APPDIR)/liquid_glass.desktop

clean:
	rm -f $(BINARY) $(DEB) liquid_icon_data.h
	rm -rf $(DEB_DIR)
