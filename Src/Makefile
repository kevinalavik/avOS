BUILD_SUBDIRS := Shell
INSTALL_SUBDIRS := Shell

.PHONY: all install clean

all:
	for dir in $(BUILD_SUBDIRS); do $(MAKE) -C "$$dir"; done

install: all
	for dir in $(INSTALL_SUBDIRS); do \
		$(MAKE) -C "$$dir" install DESTDIR="$(DESTDIR)" PREFIX="/System/Bin"; \
	done

clean:
	for dir in $(BUILD_SUBDIRS); do $(MAKE) -C "$$dir" clean; done
