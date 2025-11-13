all:
	gcc -Wall `pkg-config --cflags gtk+-2.0 lxpanel` -shared -fPIC modern_menu.c -o modernmenu.so `pkg-config --libs lxpanel`
install:
	sudo cp ./modernmenu.so /usr/lib/x86_64-linux-gnu/lxpanel/plugins/modernmenu.so

run:
	lxpanelctl restart
	@echo "add new plugin to lxpanel"
clean:
	rm -f modernmenu.so
help:
	@echo "all | install | run | help"
	@echo "see for yourself on"
