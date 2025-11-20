# ==== Configuración ====
PLUGIN_NAME = modernmenu.so
SRC = ../modern_menu.c
INSTALL_DIR = /usr/lib/i386-linux-gnu/lxpanel/plugins

CFLAGS = -Wall `pkg-config --cflags gtk+-2.0 lxpanel`
LIBS = `pkg-config --libs lxpanel`

# ==== Tareas ====
all:
	$(CC) -shared -fPIC $(CFLAGS) $(SRC) -o $(PLUGIN_NAME) $(LIBS)
	@echo "Compilado exitosamente: $(PLUGIN_NAME)"

install:
	sudo cp ./$(PLUGIN_NAME) $(INSTALL_DIR)/$(PLUGIN_NAME)
	@echo "Instalado en: $(INSTALL_DIR)/$(PLUGIN_NAME)"

run:
	lxpanelctl restart
	@echo "LXPanel reiniciado. El plugin debería estar disponible."

clean:
	rm -f $(PLUGIN_NAME)
	@echo "Limpieza completada."

help:
	@echo ""
	@echo "Uso: make [objetivo]"
	@echo ""
	@echo "Objetivos disponibles:"
	@echo "  all      -> Compila el plugin"
	@echo "  install  -> Copia el plugin al directorio de LXPanel"
	@echo "  run      -> Reinicia LXPanel para cargar el plugin"
	@echo "  clean    -> Elimina la compilación previa"
	@echo "  help     -> Muestra esta ayuda"
	@echo ""
	@echo "Directorio actual de instalación: $(INSTALL_DIR)"
