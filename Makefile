SUBDIR = src

all:
	@$(MAKE) --directory=$(SUBDIR) all
client:
	@$(MAKE) --directory=$(SUBDIR) client
lib:
	@$(MAKE) --directory=$(SUBDIR) lib
server:
	@$(MAKE) --directory=$(SUBDIR) server
clean:
	@$(MAKE) --directory=$(SUBDIR) clean
distclean:
	@$(MAKE) --directory=$(SUBDIR) distclean
install:
	@$(MAKE) --directory=$(SUBDIR) install
install-lib:
	@$(MAKE) --directory=$(SUBDIR) install-lib
install-server:
	@$(MAKE) --directory=$(SUBDIR) install-server
install-client:
	@$(MAKE) --directory=$(SUBDIR) install-client
uninstall:
	@$(MAKE) --directory=$(SUBDIR) uninstall


.PHONY: all client lib server clean distclean install install-lib install-server install-client uninstall
