# LXDE-Modern-Menu
Un menú de aplicaciones de estilo más moderno para LXDE

## Objetivo del proyecto

El objetivo de este proyecto es añadir a LXDE un menú con una apariencia más moderna e intuitiva para nuevos usuarios, sin perder la ligereza característica del entorno. El diseño está inspirado parcialmente en el estilo de KDE, e incluye:

- ⭐ Una **sección de favoritos**, que se muestra al abrir el menú.  
- Una **lista de categorías** a la izquierda.  
- Un **panel de aplicaciones** con íconos a la derecha.  
- Una **barra de búsqueda** integrada.  
- Un **botón de salida** que ejecuta el comando lxsession-logout de LXDE.
- Una **lista de ocultas** para ocultar aquellas aplicaciones que no querés borrar pero tampoco ver.

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

Clonar el repositorio con:

```bash
git clone https://github.com/tuusuario/LXDE-Modern-Menu.git
cd LXDE-Modern-Menu
```

y mover el archivo modernmenu.so a la carpeta de plugins de lxpanel, que puede ser bien con 

```bash
sudo cp ./modernmenu.so /usr/lib/lxpanel/plugins
```
o 
```bash
sudo cp ./modernmenu.so /usr/lib/x86-64-linux-gnu/lxpanel/plugins
```

dependiendo de la ruta en que se encuentre la carpeta de los plugins para lxpanel. También puedes usar el comando `make install`, cuyo funcionamiento se explica más abajo en el apartado de **Compilación**.

Al hacer **clic derecho** sobre una aplicación en el menú, se abrirá un menú contextual con las siguientes opciones:

* **Agregar a favoritos** / **Quitar de favoritos** (si ya está agregado)
* **Ocultar aplicación**
* **Agregar al escritorio** (si no lo está ya)
* **Eliminar**
* **Preferencias**

La opción *“Preferencias”* depende de la existencia del binario `lxshortcut`, que puede instalarse en sistemas basados en Debian con:

```bash
sudo apt install libfm-tools
```

## Configuración
Si se da click derecho sobre el ícono del menú aparecerá el menú contextual de lxpanel donde arriba de todo estará la opción de configurar. Si se le da click aparecerá una pequeña interfaz donde se puede hacer las siguientes acciones:

* **Modificar el ícono**
* **Modificar las aplicaciones ocultas**

Para modificar el ícono se puede o bien pegar directamente la ruta, poner el nombre (por ejemplo: app-launcher) o bien darle a **Examinar** y buscar el ícono o imagen que deseemos entre las carpetas de nuestro sistema.

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

**`make install`** copiará automáticamente el plugin a "/usr/lib/x86-64-linux-gnu/lxpanel/plugins", asegúrese de revisar la ubicación de la carpeta /lxpanel/plugins en su sistema. Una vez la encuentre modifique el archivo Makefile con la ruta correcta. Alternativamente, puede ejecutar el comando con el agregado de **`INSTALL_DIR=`** y la ruta que corresponda en su sistema

**`make run`** reiniciará lxpanel para que detecte el plugin correctamente. A veces por ejecutarlo muchas veces termina crasheando la sesión y debe de ejecutar lxsession otra vez para que funcione.

## Compilación 32 bits
En caso de compilar para 32 bits los pasos cambian un poco ya que en este caso el nombre del paquete **`libmenu-dev`**  en Debian 32 bits cambia y se usa **`libmenu-cache-dev`**, por lo que el comando quedaría así:

```bash
sudo apt install build-essential pkg-config libgtk2.0-dev lxpanel-dev libmenu-cache-dev
```

Luego de instalados los paquetes, diríjase a la carpeta **32bits** y ejecute los mismos comandos del apartado **"Compilación"**.
En este caso el Makefile contiene la ruta "/usr/lib/i386-linux-gnu/lxpanel/plugins" para el **`make install`**.

## Agradecimientos

Gracias a Uctumi de Cybercirujas por todo el feedback que me permitió mejorar este proyecto y por ayudarme con la versión de 32 bits, sin su ayuda quizás no se me habría pasado por la cabeza hacer la versión para esta arquitectura.
