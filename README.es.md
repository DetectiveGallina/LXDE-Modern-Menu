# LXDE-Modern-Menu
Un menú de aplicaciones de estilo más moderno para LXDE

### Elige tu idioma preferido
[![English](https://img.shields.io/badge/Language-English-blue)](README.md)

## Objetivo del proyecto

El objetivo de este proyecto es añadir a LXDE un menú con una apariencia más moderna e intuitiva para nuevos usuarios, sin perder la ligereza característica del entorno. El diseño está inspirado parcialmente en el estilo de KDE, e incluye:

- ⭐ Una **sección de favoritos**, que se muestra al abrir el menú.  
- Una **lista de categorías** a la izquierda.  
- Un **panel de aplicaciones** con íconos a la derecha.  
- Una **barra de búsqueda** integrada.  
- Un **botón de salida** que ejecuta el comando de logout que esté configurado en LXDE.
- Una **lista de ocultas** para ocultar aquellas aplicaciones que no querés borrar pero tampoco ver.

Un menú contextual que aparecerá al hacer **clic derecho** sobre una aplicación en el menú e incluye las siguientes opciones:

* **Agregar a favoritos** / **Quitar de favoritos** (si ya está agregado)
* **Ocultar aplicación**
* **Agregar al escritorio** (si no lo está ya)
* **Eliminar paquete**
* **Preferencias**

## Imágenes de muestra
Estas son imágenes del menú en la distro argentina Cirujantix del grupo Cybercirujas, que ya viene con el menú preinstalado:

![Imagen del menú recién abierto](img/modernmenu1.png)  
![Muestra de las opciones del menú contextual](img/modernmenu4.png)  
![Menú en 32 bits](img/modernmenu32bits.png)

## Soporte de estilos
Esta es una muestra de cómo se puede ver el menú si se tiene instalado un tema GTK, como puede ser por ejemplo uno de estilo Nord:

![Menú con tema Nord de GTK](img/modernmenuNord.png)

---
## Instalación

Para sistemas basados en Debian puedes usar directamente el archivo .deb que corresponda para tu arquitectura. Estos archivos se encuentran en el apartado de releases del repositorio.

Una vez descargado usa el siguiente comando para instalarlo en tu sistema:

En caso de desear eliminarlo puede hacerlo o bien con ```dpkg -r modernmenu```, o bien con ```apt remove modernmenu```, ambas deberían de funcionar correctamente.

## Configuración
Si se da click derecho sobre el ícono del menú aparecerá el menú contextual de lxpanel donde arriba de todo estará la opción de configurar. Si se le da click aparecerá una pequeña interfaz donde se pueden realizar las siguientes acciones:

* **Modificar el ícono**
* **Modificar las aplicaciones ocultas**

Para modificar el ícono se puede o bien pegar directamente la ruta, poner el nombre del icono (por ejemplo: app-launcher) o bien darle a **Examinar** y buscar el ícono o imagen que quiera entre las carpetas de nuestro sistema.

Para modificar las aplicaciones ocultas tenemos el botón de **Gestionar**, que al darle click nos aparecerá otra ventana donde podremos ver cuáles aplicaciones están ocultas y al lado la opción de **Mostrar** para que dejen de estarlo.

## Compilación
Para compilarlo primero asegúrese de instalar los siguientes paquetes

```bash
sudo apt install build-essential pkg-config libgtk2.0-dev lxpanel-dev libmenu-dev
```

y luego, estando en la carpeta, ejecute 

```bash
make
make install
make run
```

**`make install`** copiará automáticamente el plugin a la ruta donde se encuentre la carpeta lxpanel/plugins.

**`make run`** reiniciará lxpanel para que detecte el plugin correctamente. A veces por ejecutarlo muchas veces termina crasheando la sesión y debe de ejecutar lxsession otra vez para que funcione.

## Compilación 32 bits
En caso de compilar para 32 bits los pasos cambian un poco ya que en este caso el nombre del paquete **`libmenu-dev`**  en Debian 32 bits cambia y se usa **`libmenu-cache-dev`**, por lo que el comando quedaría así:

```bash
sudo apt install build-essential pkg-config libgtk2.0-dev lxpanel-dev libmenu-cache-dev
```

Luego de instalados los paquetes, ejecute **`make 32bits`** y para instalar **`make install-32bits`**.

## Empaquetado
El repositorio incluye un script para crear paquetes `.deb` con las siguientes opciones:

Crear paquete para 64 bits
```bash
./create-deb.sh
```
Crear paquete para 32 bits
```bash
./create-deb.sh 32bits
```
Limpiar paquetes generados
```bash
./create-deb.sh clean
```

## Traducciones
El menú soporta múltiples idiomas mediante gettext. Los archivos de traducción están en la carpeta `po/`.

### Agregar un nuevo idioma:
```bash
make new-lang LANG=codigo
# Ejemplo: make new-lang LANG=fr
# Editar po/fr.po
make update-po
```

### Idiomas disponibles:
- Español (es)
- Inglés (default)
- Portugués (pt)

## Agradecimientos

Gracias a Uctumi de Cybercirujas por todo el feedback que me permitió mejorar este proyecto y por ayudarme con la versión de 32 bits, sin su ayuda quizás no se me habría pasado por la cabeza hacer la versión para esta arquitectura.

Gracias a Nico de Locos por Linux por la ayuda en los directos y por incluir el menú en Loc-OS 24, gracias a esto me puse las pilas para agregar el soporte a traducciones.
