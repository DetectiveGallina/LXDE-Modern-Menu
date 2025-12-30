#!/bin/bash
# Script para crear paquetes .deb de Modern Menu

set -e  # Salir en error

# Valores por defecto
DEB_NAME="modernmenu"
DEFAULT_VERSION="1.0.0"
DEFAULT_MAINTAINER="$(git config user.name 2>/dev/null || echo "Your name") <$(git config user.email 2>/dev/null || echo "your@email.com")>"
DEFAULT_DEPENDS="lxpanel, libfm"
DEB_DESCRIPTION="Modern menu plugin for LXPanel"

# Mostrar información actual y dependencias
echo "=== Configuración actual ==="
echo "Versión: $DEFAULT_VERSION"
echo "Mantenedor: $DEFAULT_MAINTAINER"
echo "Dependencias: $DEFAULT_DEPENDS"
echo ""
echo "=== Dependencias detectadas en el código ==="
if [ -f "src/modern_menu.c" ]; then
    echo "Archivos incluidos en modern_menu.c:"
    grep "#include <" src/modern_menu.c | grep -E "(gtk|glib|lxpanel|menu-cache|libfm)" | \
        sed 's/.*<\([^>]*\)>.*/\1/' | sort -u | sed 's/^/  - /'
fi
echo ""

# Pedir configuración al usuario
echo "Configurar paquete .deb (presiona Enter para usar valores por defecto):"
read -p "Versión [$DEFAULT_VERSION]: " input_version
DEB_VERSION="${input_version:-$DEFAULT_VERSION}"

read -p "Mantenedor [$DEFAULT_MAINTAINER]: " input_maintainer
DEB_MAINTAINER="${input_maintainer:-$DEFAULT_MAINTAINER}"

read -p "Dependencias [$DEFAULT_DEPENDS]: " input_depends
DEB_DEPENDS="${input_depends:-$DEFAULT_DEPENDS}"

echo ""
echo "Resumen:"
echo "  Versión: $DEB_VERSION"
echo "  Mantenedor: $DEB_MAINTAINER"
echo "  Dependencias: $DEB_DEPENDS"
echo ""

read -p "¿Continuar? [S/n]: " confirm
if [[ "$confirm" =~ ^[Nn]$ ]]; then
    echo "Cancelado."
    exit 0
fi

# Función para crear paquete .deb (todo el resto del código se mantiene igual)
create_deb() {
    local arch="$1"
    local package_dir="${DEB_NAME}_${DEB_VERSION}_${arch}"

    echo "=== Creando paquete .deb para ${arch} ==="

    # Limpiar
    rm -rf "$package_dir"
    mkdir -p "$package_dir/DEBIAN"

    # Buscar el plugin compilado
    local plugin_source=""
    if [ "$arch" = "i386" ]; then
        plugin_source="build/32bits/modernmenu.so"
    else
        plugin_source="build/modernmenu.so"
    fi

    if [ ! -f "$plugin_source" ]; then
        echo "Error: Plugin $arch no encontrado en $plugin_source"
        echo "Ejecuta primero: make all"
        [ "$arch" = "i386" ] && echo "Para 32 bits necesitas: make 32bits"
        exit 1
    fi

    echo "Plugin fuente: $plugin_source"

    # Copiar plugin a ubicación temporal
    mkdir -p "$package_dir/usr/share/modernmenu"
    cp "$plugin_source" "$package_dir/usr/share/modernmenu/modernmenu.so"

    # Copiar traducciones si existen
    if [ -d "po" ]; then
        echo "Copiando traducciones..."
        for po_file in po/*.po; do
            [ -f "$po_file" ] || continue
            lang=$(basename "$po_file" .po)
            mo_dir="$package_dir/usr/share/locale/$lang/LC_MESSAGES"
            mkdir -p "$mo_dir"
            msgfmt -o "$mo_dir/modernmenu.mo" "$po_file" 2>/dev/null || true
        done
    fi

    # Calcular tamaño
    local installed_size=$(du -sk "$package_dir" | cut -f1)

    # Crear archivo control
    cat > "$package_dir/DEBIAN/control" <<EOF
Package: ${DEB_NAME}
Version: ${DEB_VERSION}
Architecture: ${arch}
Maintainer: ${DEB_MAINTAINER}
Installed-Size: ${installed_size}
Depends: ${DEB_DEPENDS}
Description: ${DEB_DESCRIPTION}
 Modern applications menu with search and favorites.
EOF

    # Crear script postinst
    cat > "$package_dir/DEBIAN/postinst" <<'EOF'
#!/bin/bash
# Script de post-instalación
set -e

PLUGIN_SOURCE="/usr/share/modernmenu/modernmenu.so"

echo "=== Instalando Modern Menu ==="

# Función para encontrar el directorio de plugins
find_plugins_dir() {
    local arch="$1"

    if [ "$arch" = "64" ]; then
        # Orden de búsqueda para 64 bits
        if [ -d "/usr/lib/lxpanel/plugins" ]; then
            echo "/usr/lib/lxpanel/plugins"
        elif [ -d "/usr/lib/x86_64-linux-gnu/lxpanel/plugins" ]; then
            echo "/usr/lib/x86_64-linux-gnu/lxpanel/plugins"
        elif [ -d "/usr/lib64/lxpanel/plugins" ]; then
            echo "/usr/lib64/lxpanel/plugins"
        else
            mkdir -p "/usr/lib/lxpanel/plugins"
            echo "/usr/lib/lxpanel/plugins"
        fi
    else
        # Orden de búsqueda para 32 bits
        if [ -d "/usr/lib/lxpanel/plugins" ]; then
            echo "/usr/lib/lxpanel/plugins"
        elif [ -d "/usr/lib/i386-linux-gnu/lxpanel/plugins" ]; then
            echo "/usr/lib/i386-linux-gnu/lxpanel/plugins"
        elif [ -d "/usr/lib32/lxpanel/plugins" ]; then
            echo "/usr/lib32/lxpanel/plugins"
        else
            mkdir -p "/usr/lib/lxpanel/plugins"
            echo "/usr/lib/lxpanel/plugins"
        fi
    fi
}

# Determinar arquitectura del paquete por el nombre del archivo
if [ -f "/usr/share/modernmenu/modernmenu.so" ]; then
    # Verificar si es ejecutable de 32 bits
    if file "/usr/share/modernmenu/modernmenu.so" | grep -q "ELF 32-bit"; then
        ARCH="32"
    else
        ARCH="64"
    fi
else
    # Por defecto según arquitectura del sistema
    if [ "$(uname -m)" = "x86_64" ]; then
        ARCH="64"
    else
        ARCH="32"
    fi
fi

echo "Arquitectura del plugin: $ARCH-bit"

# Buscar directorio
INSTALL_DIR=$(find_plugins_dir "$ARCH")
echo "Directorio de plugins: $INSTALL_DIR"

# Copiar plugin
cp "$PLUGIN_SOURCE" "$INSTALL_DIR/modernmenu.so"
chmod 644 "$INSTALL_DIR/modernmenu.so"
echo "✓ Plugin instalado en: $INSTALL_DIR/modernmenu.so"

# Limpiar
rm -f "$PLUGIN_SOURCE"
rmdir "/usr/share/modernmenu" 2>/dev/null || true

# Reiniciar LXPanel
if command -v lxpanelctl >/dev/null 2>&1; then
    echo "Reiniciando LXPanel..."
    lxpanelctl restart || true
fi

echo "✓ Instalación completada"
exit 0
EOF
    chmod 755 "$package_dir/DEBIAN/postinst"

 # Crear script postrm (DESPUÉS de quitar)
    cat > "$package_dir/DEBIAN/postrm" <<'EOF'
#!/bin/bash
# Post-remove script - Se ejecuta DESPUÉS de quitar el paquete
set -e

echo "=== Finalizando remoción de Modern Menu ==="

# Buscar y eliminar el archivo del plugin
remove_plugin_file() {
    # Buscar en todas las ubicaciones posibles
    local locations="
        /usr/lib/lxpanel/plugins/modernmenu.so
        /usr/lib/x86_64-linux-gnu/lxpanel/plugins/modernmenu.so
        /usr/lib/i386-linux-gnu/lxpanel/plugins/modernmenu.so
        /usr/lib64/lxpanel/plugins/modernmenu.so
        /usr/lib32/lxpanel/plugins/modernmenu.so
    "

    for file in $locations; do
        if [ -f "$file" ]; then
            echo "Eliminando: $file"
            rm -f "$file"
        fi
    done
}

# Solo si estamos removiendo completamente, no en upgrade
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    # Eliminar archivo del plugin
    remove_plugin_file

    # Limpiar archivos temporales
    rm -f "/usr/share/modernmenu/modernmenu.so" 2>/dev/null || true
    rmdir "/usr/share/modernmenu" 2>/dev/null || true

    # Reiniciar LXPanel para que detecte que el plugin ya no está
    if command -v lxpanelctl >/dev/null 2>&1; then
        echo "Reiniciando LXPanel..."
        lxpanelctl restart || true
        echo "✓ LXPanel reiniciado"
    fi

    echo "✓ Modern Menu removido completamente"
fi

# Si es purge, también eliminar configuraciones
if [ "$1" = "purge" ]; then
    echo "Eliminando configuraciones..."
    rm -rf "/home/*/.config/modernmenu" 2>/dev/null || true
    rm -rf "/root/.config/modernmenu" 2>/dev/null || true
    echo "✓ Configuraciones eliminadas"
fi

exit 0
EOF
    chmod 755 "$package_dir/DEBIAN/postrm"

    # Construir paquete
    dpkg-deb --build --root-owner-group "$package_dir"

    if [ -f "${package_dir}.deb" ]; then
        echo "✓ Paquete creado: ${package_dir}.deb"
    else
        echo "Error: No se pudo crear el paquete"
        exit 1
    fi
}

# Mostrar ayuda
show_help() {
    echo "Uso: $0 [64bits|32bits]"
    echo ""
    echo "Opciones:"
    echo "  64bits    Crea paquete para arquitectura 64 bits"
    echo "  32bits    Crea paquete para arquitectura 32 bits"
    echo "  clean     Elimina todos los paquetes generados"
    echo ""
    echo "NOTA: Primero debes compilar el plugin:"
    echo "  make all        # Para 64 bits"
    echo "  make 32bits     # Para 32 bits"
}

# Función principal
main() {
    case "$1" in
        "64bits"|"")
            # Por defecto o explícitamente 64 bits
            echo "=== Generando paquete 64 bits ==="

            # Verificar si está compilado
            if [ ! -f "build/modernmenu.so" ]; then
                echo "Compilando plugin 64 bits..."
                make all
            fi

            create_deb "amd64"
            echo ""
            echo "Paquete listo: modernmenu_${DEB_VERSION}_amd64.deb"
            ;;

        "32bits")
            echo "=== Generando paquete 32 bits ==="

            # Verificar si está compilado
            if [ ! -f "build/32bits/modernmenu.so" ]; then
                echo "Compilando plugin 32 bits..."
                make 32bits
            fi

            create_deb "i386"
            echo ""
            echo "Paquete listo: modernmenu_${DEB_VERSION}_i386.deb"
            ;;

        "clean")
            echo "Limpiando paquetes..."
            rm -rf modernmenu_*.deb modernmenu_*_*
            echo "✓ Limpieza completada"
            ;;

        "--help"|"-h"|"help")
            show_help
            ;;

        *)
            echo "Error: Opción no válida: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# Verificar dpkg-deb
if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "Error: dpkg-deb no encontrado."
    echo "Instala: sudo apt install dpkg"
    exit 1
fi

# Ejecutar
main "$1"
