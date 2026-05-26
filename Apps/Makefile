BUILD_SUBDIRS := Terminal Showcase Test Doom
INSTALL_SUBDIRS := Terminal Showcase Test Doom

.PHONY: all install clean

all:
	for dir in $(BUILD_SUBDIRS); do $(MAKE) -C "$$dir"; done

install: all
	for dir in $(INSTALL_SUBDIRS); do \
		$(MAKE) -C "$$dir" install DESTDIR="$(DESTDIR)" PREFIX="$(PREFIX)"; \
	done

clean:
	for dir in $(BUILD_SUBDIRS); do $(MAKE) -C "$$dir" clean; done
