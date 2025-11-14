# LXDE-Modern-Menu
Un menú de aplicaciones de estilo más moderno para lxde

## Objetivo del proyecto

El objetivo de este proyecto es añadir a LXDE un menú con una apariencia más moderna e intuitiva para nuevos usuarios, sin perder la ligereza característica del entorno. El diseño está inspirado parcialmente en el estilo de KDE, e incluye:

- ⭐ Una **sección de favoritos**, que se muestra al abrir el menú.  
- Una **lista de categorías** a la izquierda.  
- Un **panel de aplicaciones** con íconos a la derecha.  
- Una **barra de búsqueda** integrada.  
- Un **botón de salida** que ejecuta el comando lxsession-logout de LXDE.

---

## Instalación

Clonar el repositorio con:

```bash
git clone https://github.com/tuusuario/LXDE-Modern-Menu.git
cd LXDE-Modern-Menu
```

y mover el archivo modernmenu.so a la carptea de plugins de lxpanel, que puede ser bien con 

```bash
sudo cp ./modernmenu.so /usr/lib/lxpanel/plugins
```
o 
```bash
sudo cp ./modernmenu.so /usr/lib/x86-64-linux-gnu/lxpanel/plugins
```

dependiendo de la ruta en que se encuentre la carpeta de los plugins para lxpanel.
Al hacer **clic derecho** sobre una aplicación en el menú, se abrirá un menú contextual con las siguientes opciones:

* **Agregar a favoritos** / **Quitar de favoritos** (si ya está agregado)
* **Añadir al escritorio** (si no lo está ya)
* **Eliminar**
* **Preferencias**

La opción *“Preferencias”* depende de la existencia del binario **`lxshortcut`**, que puede instalarse en bases debian con:

```bash
sudo apt install libfm-tools
```

## Compilación
para compilarlo primero asegurese de isntalar los siguientes paquetes

```bash
sudo apt install build-essential pkg-config libgtk2.0-dev lxpanel-dev libmenu-dev
```

y luego, estando en la carpeta, ejecute 

```bash
make
make install
make run
```

**`make install`** copiará automaticamente el plugin a "/usr/lib/x86-64-linux-gnu/lxpanel/plugins", aseguresé de revisar la ubicación de la carpeta /lxpanel/plugins en su sistema. Una vez la encuentre modifique el archivo Makefile con la ruta correcta. Alternativamente, puede ejecutar el comando con el agregado de **`INSTALL_DIR=`** y la ruta que corresponda en su sistema

**`make run`** reiniciará lxpanel para que detecte el plugin correctamente. A veces por ejecutarlo muchas veces termina crasheando la sesión y debe de ejecutar lxsession otra vez para que funcione.
