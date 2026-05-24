CC       = gcc
CFLAGS   = -O2
PKGFLAGS = $(shell pkg-config --cflags --libs gtk+-3.0 vte-2.91 epoxy gl)
LIBS     = -lX11 -lXcomposite -lXrender -lm

BINARY   = liquid_glass_gtk
VERSION  = 1.0.0
ARCH     = amd64
DEB      = liquid-glass_$(VERSION)_$(ARCH).deb
DEB_DIR  = .debpkg
XFCE_DEB     = liquid-glass-xfwm4_$(VERSION)_$(ARCH).deb
XFCE_DEB_DIR = .debpkg-xfwm4

PREFIX   = /usr
BINDIR   = $(PREFIX)/bin
ICONDIR  = $(PREFIX)/share/icons/hicolor/256x256/apps
APPDIR   = $(PREFIX)/share/applications
DOCDIR   = $(PREFIX)/share/doc/liquid-glass
XFWM4_REPO = https://gitlab.xfce.org/xfce/xfwm4.git
XFWM4_SRC  = .xfwm4-src
XFWM4_BUILD = $(XFWM4_SRC)/build-liquidglass
XFWM4_PATCH = $(CURDIR)/xfwm4-native-effect/liquidglass-xfwm4.patch

.PHONY: all binary deb deb-xfce clean install uninstall kwin-native-build kwin-native-install xfce-native-fetch xfce-native-build xfce-native-install

all: binary

liquid_icon_data.h: liquid.png
	xxd -i liquid.png > liquid_icon_data.h

binary: liquid_icon_data.h liquid_glass_gtk.c backdrop_refraction.c backdrop_refraction.h
	$(CC) $(CFLAGS) -DEMBED_ICON -o $(BINARY) liquid_glass_gtk.c backdrop_refraction.c $(PKGFLAGS) $(LIBS)

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
	printf 'Exec=$(BINDIR)/$(BINARY)\nIcon=liquid_glass\nTerminal=false\n' >> $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop
	printf 'Type=Application\nCategories=System;TerminalEmulator;\n'            >> $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop
	printf 'StartupWMClass=$(BINARY)\n'                                         >> $(DEB_DIR)/$(APPDIR)/liquid_glass.desktop

	printf 'Liquid Glass Terminal\nCopyright (C) 2026 Chokri Hammedi\nAll rights reserved.\n' \
	    > $(DEB_DIR)/$(DOCDIR)/copyright

	printf 'Package: liquid-glass\nVersion: $(VERSION)\nArchitecture: $(ARCH)\n'       > $(DEB_DIR)/DEBIAN/control
	printf 'Maintainer: Chokri Hammedi <chokrihammedi51@gmail.com>\n'                  >> $(DEB_DIR)/DEBIAN/control
	printf 'Depends: libgtk-3-0, libvte-2.91-0, libx11-6, libxcomposite1, libxrender1, libepoxy0, libgl1\n'          >> $(DEB_DIR)/DEBIAN/control
	printf 'Section: x11\nPriority: optional\n'                                        >> $(DEB_DIR)/DEBIAN/control
	printf 'Homepage: https://github.com/blue0x1/liquid-glass\n'                       >> $(DEB_DIR)/DEBIAN/control
	printf 'Description: Liquid Glass Terminal\n'                                      >> $(DEB_DIR)/DEBIAN/control
	printf ' A GTK3 terminal emulator with a liquid glass aesthetic.\n'                >> $(DEB_DIR)/DEBIAN/control
	printf ' Features an OpenGL liquid shader, translucent frosted-glass\n'            >> $(DEB_DIR)/DEBIAN/control
	printf ' panels, KWin blur-behind support, tabbed sessions, a collapsible\n'       >> $(DEB_DIR)/DEBIAN/control
	printf ' sidebar, and a live theme/opacity settings window.\n'                     >> $(DEB_DIR)/DEBIAN/control

	printf '#!/bin/sh\nset -e\n'                                                        > $(DEB_DIR)/DEBIAN/postinst
	printf 'gtk-update-icon-cache -f -t /usr/share/icons/hicolor/ 2>/dev/null || true\n' >> $(DEB_DIR)/DEBIAN/postinst
	printf 'update-desktop-database /usr/share/applications 2>/dev/null || true\n'    >> $(DEB_DIR)/DEBIAN/postinst

	chmod 755 $(DEB_DIR)/DEBIAN/postinst
	chmod 755 $(DEB_DIR)/$(BINDIR)/$(BINARY)
	dpkg-deb --root-owner-group --build $(DEB_DIR) $(DEB)
	rm -rf $(DEB_DIR)
	@echo ""
	@echo "Built: $(DEB)"

deb-xfce: xfce-native-build
	rm -rf $(XFCE_DEB_DIR)
	mkdir -p $(XFCE_DEB_DIR)/DEBIAN
	mkdir -p $(XFCE_DEB_DIR)/usr/local/bin
	mkdir -p $(XFCE_DEB_DIR)/usr/share/doc/liquid-glass-xfwm4

	cp $(XFWM4_BUILD)/src/xfwm4 $(XFCE_DEB_DIR)/usr/local/bin/xfwm4

	printf 'Liquid Glass patched xfwm4 compositor\nCopyright (C) 2026 Chokri Hammedi\nAll rights reserved.\n' \
	    > $(XFCE_DEB_DIR)/usr/share/doc/liquid-glass-xfwm4/copyright

	printf 'Package: liquid-glass-xfwm4\nVersion: $(VERSION)\nArchitecture: $(ARCH)\n'          > $(XFCE_DEB_DIR)/DEBIAN/control
	printf 'Maintainer: Chokri Hammedi <chokrihammedi51@gmail.com>\n'                            >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf 'Depends: libxfce4ui-2-0, libxfce4util7, libglib2.0-0, libgtk-3-0, libxrender1, libxcomposite1, libxdamage1, libxfixes3, libxrandr2\n' >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf 'Section: x11\nPriority: optional\n'                                                  >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf 'Homepage: https://github.com/blue0x1/liquid-glass\n'                                 >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf 'Description: Liquid Glass patched xfwm4 compositor\n'                               >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf ' Patched xfwm4 compositor that renders compositor-side liquid glass\n'              >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf ' refraction for the Liquid Glass Terminal on XFCE desktops.\n'                      >> $(XFCE_DEB_DIR)/DEBIAN/control
	printf ' Installs to /usr/local/bin/xfwm4, shadowing the system compositor.\n'              >> $(XFCE_DEB_DIR)/DEBIAN/control

	printf '#!/bin/sh\nset -e\n'                                                                  > $(XFCE_DEB_DIR)/DEBIAN/postinst
	printf 'echo "liquid-glass-xfwm4: installed to /usr/local/bin/xfwm4"\n'                     >> $(XFCE_DEB_DIR)/DEBIAN/postinst
	printf 'echo "  Run: xfwm4 --replace & (inside your XFCE session) to activate."\n'         >> $(XFCE_DEB_DIR)/DEBIAN/postinst

	chmod 755 $(XFCE_DEB_DIR)/DEBIAN/postinst
	chmod 755 $(XFCE_DEB_DIR)/usr/local/bin/xfwm4
	dpkg-deb --root-owner-group --build $(XFCE_DEB_DIR) $(XFCE_DEB)
	rm -rf $(XFCE_DEB_DIR)
	@echo ""
	@echo "Built: $(XFCE_DEB)"
	@echo "Install: sudo dpkg -i $(XFCE_DEB)"
	@echo "Activate: xfwm4 --replace &"

install: binary
	@if command -v cmake >/dev/null 2>&1; then $(MAKE) kwin-native-install; fi
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
	rm -f $(BINARY) $(DEB) $(XFCE_DEB) liquid_icon_data.h
	rm -rf $(DEB_DIR) $(XFCE_DEB_DIR)
	rm -rf kwin-native-effect/build
	rm -rf $(XFWM4_BUILD)

kwin-native-build:
	cmake -S kwin-native-effect -B kwin-native-effect/build
	cmake --build kwin-native-effect/build

kwin-native-install: kwin-native-build
	cmake --install kwin-native-effect/build --prefix /usr
	@if [ -n "$$SUDO_USER" ] && [ "$$SUDO_USER" != "root" ]; then \
		sudo -u "$$SUDO_USER" kwriteconfig6 --file kwinrc --group Plugins --key liquidglassnativeEnabled true; \
		sudo -u "$$SUDO_USER" qdbus6 org.kde.KWin /KWin reconfigure || true; \
	else \
		kwriteconfig6 --file kwinrc --group Plugins --key liquidglassnativeEnabled true; \
		qdbus6 org.kde.KWin /KWin reconfigure || true; \
	fi

xfce-native-fetch:
	if [ ! -d "$(XFWM4_SRC)/.git" ]; then \
		git clone $(XFWM4_REPO) $(XFWM4_SRC); \
	fi

xfce-native-build: xfce-native-fetch
	@cd $(XFWM4_SRC) && \
	if git apply --reverse --check $(XFWM4_PATCH) 2>/dev/null; then \
		echo "xfwm4 patch already applied"; \
	else \
		git apply $(XFWM4_PATCH); \
	fi
	if [ -d "$(XFWM4_BUILD)" ]; then \
		meson setup --reconfigure $(XFWM4_BUILD) $(XFWM4_SRC); \
	else \
		meson setup $(XFWM4_BUILD) $(XFWM4_SRC); \
	fi
	meson compile -C $(XFWM4_BUILD)

xfce-native-install: xfce-native-build
	sudo ninja -C $(XFWM4_BUILD) install
