# ==== Configuración ====
PLUGIN_NAME = modernmenu.so
SRC = src/modern_menu.c

# Detectar arquitectura
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    NATIVE_BITS = 64
    NATIVE_LIB_DIR = /usr/lib/x86_64-linux-gnu
else ifeq ($(ARCH),i686)
    NATIVE_BITS = 32
    NATIVE_LIB_DIR = /usr/lib/i386-linux-gnu
else ifeq ($(ARCH),i386)
    NATIVE_BITS = 32
    NATIVE_LIB_DIR = /usr/lib/i386-linux-gnu
else
    NATIVE_BITS = unknown
    NATIVE_LIB_DIR = /usr/lib
endif

# Búsqueda exhaustiva del directorio de plugins
define find_lxpanel_plugins
$(shell \
    for dir in \
        /usr/lib/x86_64-linux-gnu/lxpanel/plugins \
        /usr/lib/i386-linux-gnu/lxpanel/plugins \
        /usr/lib/lxpanel/plugins \
        /usr/lib64/lxpanel/plugins \
        /usr/lib32/lxpanel/plugins \
        /usr/local/lib/lxpanel/plugins \
        /usr/local/lib64/lxpanel/plugins; \
    do \
        if [ -d "$$dir" ]; then \
            echo "$$dir"; \
            break; \
        fi; \
    done; \
    # Fallback: buscar en todo /usr \
    if [ -z "$$found" ]; then \
        find /usr -type d -name "plugins" 2>/dev/null | \
        grep -E "(lxpanel|liblxpanel)" | \
        head -1; \
    fi \
)
endef

INSTALL_DIR := $(call find_lxpanel_plugins)
ifeq ($(INSTALL_DIR),)
    # Fallback final por arquitectura
    ifeq ($(NATIVE_BITS),64)
        INSTALL_DIR = /usr/lib/x86_64-linux-gnu/lxpanel/plugins
    else ifeq ($(NATIVE_BITS),32)
        INSTALL_DIR = /usr/lib/i386-linux-gnu/lxpanel/plugins
    else
        INSTALL_DIR = /usr/lib/lxpanel/plugins
    endif
endif

# Flags para gettext
GETTEXT_FLAGS = -DENABLE_NLS

# Dependencias
CFLAGS = -Wall -fPIC $(GETTEXT_FLAGS) `pkg-config --cflags gtk+-2.0 lxpanel`
LIBS = `pkg-config --libs lxpanel`

# Directorios para traducciones
LOCALEDIR = /usr/share/locale
PO_DIR = po
PO_FILES = $(wildcard $(PO_DIR)/*.po)

# Directorios de salida
BUILD_DIR = build
NATIVE_OUTPUT_DIR = $(BUILD_DIR)
32BIT_OUTPUT_DIR = $(BUILD_DIR)/32bits
PLUGIN_PATH = $(NATIVE_OUTPUT_DIR)/$(PLUGIN_NAME)

# ==== Tareas Principales ====
all: $(PLUGIN_PATH)

# Mostrar información de detección
detect:
	@echo "=== Sistema detectado ==="
	@echo "Arquitectura: $(ARCH)"
	@echo "Bits nativos: $(NATIVE_BITS)"
	@echo "Directorio lib nativo: $(NATIVE_LIB_DIR)"
	@echo "Directorio de instalación encontrado: $(INSTALL_DIR)"
	@echo ""
	@echo "=== Directorios posibles de plugins ==="
	@for dir in \
        /usr/lib/x86_64-linux-gnu/lxpanel/plugins \
        /usr/lib/i386-linux-gnu/lxpanel/plugins \
        /usr/lib/lxpanel/plugins; \
    do \
        if [ -d "$$dir" ]; then \
            echo "✓ $$dir (existe)"; \
        else \
            echo "  $$dir (no existe)"; \
        fi; \
    done

# Compilar para arquitectura nativa
$(NATIVE_OUTPUT_DIR)/$(PLUGIN_NAME): $(SRC)
	@mkdir -p $(NATIVE_OUTPUT_DIR)
	$(CC) -shared -fPIC $(CFLAGS) $(SRC) -o $@ $(LIBS)
	@echo "✓ Plugin $(NATIVE_BITS)-bits compilado: $@"
	@echo "  Instalación: $(INSTALL_DIR)"

# ==== COMPILACIÓN 32 BITS (Cruzada o nativa) ====
32bits: clean-32bits
	@echo "=== Compilando para 32 bits ==="
	@if [ "$(NATIVE_BITS)" = "32" ]; then \
		$(MAKE) $(NATIVE_OUTPUT_DIR)/$(PLUGIN_NAME); \
		mkdir -p $(32BIT_OUTPUT_DIR); \
		mv $(NATIVE_OUTPUT_DIR)/$(PLUGIN_NAME) $(32BIT_OUTPUT_DIR)/; \
		echo "✓ Plugin 32 bits compilado nativamente"; \
	else \
		if command -v i686-linux-gnu-gcc >/dev/null 2>&1; then \
			$(MAKE) cross-32bits; \
		elif command -v gcc -m32 >/dev/null 2>&1; then \
			$(MAKE) native-32bits; \
		else \
			echo "Error: No toolchain para 32 bits encontrada"; \
			echo "Instala: sudo apt install gcc-multilib libc6-dev-i386"; \
			exit 1; \
		fi; \
		echo "✓ Plugin 32 bits compilado (cross-compilation)"; \
	fi

# Compilación cruzada con toolchain i686
cross-32bits:
	@mkdir -p $(32BIT_OUTPUT_DIR)
	i686-linux-gnu-gcc -shared -fPIC \
		-DENABLE_NLS \
		`i686-linux-gnu-pkg-config --cflags gtk+-2.0 lxpanel 2>/dev/null || pkg-config --cflags gtk+-2.0 lxpanel` \
		$(SRC) -o $(32BIT_OUTPUT_DIR)/$(PLUGIN_NAME) \
		`i686-linux-gnu-pkg-config --libs lxpanel 2>/dev/null || pkg-config --libs lxpanel`

# Compilación nativa con -m32
native-32bits:
	@mkdir -p $(32BIT_OUTPUT_DIR)
	gcc -shared -fPIC -m32 \
		-DENABLE_NLS \
		`pkg-config --cflags gtk+-2.0 lxpanel` \
		$(SRC) -o $(32BIT_OUTPUT_DIR)/$(PLUGIN_NAME) \
		`pkg-config --libs lxpanel`

# ==== TRADUCCIONES ====
modernmenu.pot: $(SRC)
	@mkdir -p $(PO_DIR)
	xgettext --keyword=_ --keyword=N_ \
	         --from-code=UTF-8 \
	         --output=$(PO_DIR)/modernmenu.pot \
	         $(SRC)
	@echo "✓ Plantilla de traducción: $(PO_DIR)/modernmenu.pot"

update-po: modernmenu.pot
	@for po in $(PO_FILES); do \
		echo "Actualizando $$po..."; \
		msgmerge --update "$$po" $(PO_DIR)/modernmenu.pot; \
	done
	@echo "✓ Archivos .po actualizados"

# ==== Instalación ====
install: all
	@echo "=== Instalando plugin ==="
	@echo "Arquitectura: $(NATIVE_BITS)-bits"
	@echo "Directorio: $(INSTALL_DIR)"

	# Crear directorio si no existe
	install -d $(DESTDIR)$(INSTALL_DIR)

	# Instalar plugin
	install -m 644 $(PLUGIN_PATH) $(DESTDIR)$(INSTALL_DIR)/$(PLUGIN_NAME)
	@echo "✓ Plugin instalado: $(DESTDIR)$(INSTALL_DIR)/$(PLUGIN_NAME)"

	# Instalar traducciones
	@if [ -d "$(PO_DIR)" ]; then \
		for po in $(PO_FILES); do \
			lang=$$(basename $$po .po); \
			install -d $(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES; \
			msgfmt -o $(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/modernmenu.mo $$po 2>/dev/null && \
				echo "  ✓ Traducción $$lang instalada" || \
				echo "  ⚠  Error en traducción $$lang"; \
		done; \
	fi

# Instalar específicamente para 32 bits
install-32bits: 32bits
	@echo "=== Instalando plugin 32 bits ==="

	# Directorios posibles para 32 bits
	32BIT_DIRS = /usr/lib/i386-linux-gnu/lxpanel/plugins /usr/lib/lxpanel/plugins
	32BIT_INSTALL_DIR := $(shell \
		for dir in $(32BIT_DIRS); do \
			if [ -d "$$dir" ]; then \
				echo "$$dir"; \
				break; \
			fi; \
		done \
	)

	@if [ -z "$(32BIT_INSTALL_DIR)" ]; then \
		echo "Error: No se encontró directorio para 32 bits"; \
		exit 1; \
	fi

	@echo "Directorio 32 bits: $(32BIT_INSTALL_DIR)"
	install -d $(DESTDIR)$(32BIT_INSTALL_DIR)
	install -m 644 $(32BIT_OUTPUT_DIR)/$(PLUGIN_NAME) $(DESTDIR)$(32BIT_INSTALL_DIR)/$(PLUGIN_NAME)
	@echo "✓ Plugin 32 bits instalado: $(DESTDIR)$(32BIT_INSTALL_DIR)/$(PLUGIN_NAME)"
run:
	lxpanelctl restart
	@echo "LXPanel reiniciado. El plugin debería estar disponible."
# ==== Utilidades ====
new-lang:
ifndef LANG
	$(error Use: make new-lang LANG=código (ej: make new-lang LANG=de))
endif
	@mkdir -p $(PO_DIR)
	@if [ ! -f "$(PO_DIR)/modernmenu.pot" ]; then $(MAKE) modernmenu.pot; fi
	msginit --input=$(PO_DIR)/modernmenu.pot --output=$(PO_DIR)/$(LANG).po --locale=$(LANG).UTF-8
	@echo "✓ Nuevo idioma: $(PO_DIR)/$(LANG).po"

list-langs:
	@echo "Idiomas disponibles:"
	@for po in $(PO_FILES); do \
		lang=$$(basename $$po .po); \
		echo "  • $$lang"; \
	done

# ==== Limpieza ====
clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ Build limpiado"

clean-32bits:
	rm -rf $(32BIT_OUTPUT_DIR)
	@echo "✓ Build 32 bits limpiado"

distclean: clean clean-32bits
	rm -f $(PO_DIR)/modernmenu.pot
	@echo "✓ Limpieza total"

# ==== Ayuda ====
help:
	@echo "Modern Menu - Sistema de compilación"
	@echo ""
	@echo "Comandos principales:"
	@echo "  make all           - Compilar plugin para arquitectura nativa"
	@echo "  make 32bits        - Compilar para 32 bits"
	@echo "  make detect        - Mostrar información del sistema"
	@echo "  make install       - Instalar plugin"
	@echo "  make install-32bits - Instalar versión 32 bits"
	@echo ""
	@echo "Traducciones:"
	@echo "  make modernmenu.pot - Generar plantilla de traducción"
	@echo "  make update-po      - Actualizar archivos .po"
	@echo "  make new-lang LANG=código - Crear nuevo idioma"
	@echo "  make list-langs     - Listar idiomas disponibles"
	@echo ""
	@echo "Limpieza:"
	@echo "  make clean         - Limpiar builds"
	@echo "  make distclean     - Limpieza completa"
	@echo ""
	@echo "Para crear paquete .deb, ejecuta: ./create-deb.sh"

.PHONY: all detect 32bits cross-32bits native-32bits install install-32bits \
        clean clean-32bits distclean update-po new-lang list-langs help modernmenu.pot
