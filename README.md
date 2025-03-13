# DWIMSH - Do What I Mean Shell

## Descripción
DWIMSH es un shell interactivo para Linux que ayuda a los usuarios a corregir errores tipográficos en comandos mediante:
- Distancia de Hamming
- Distancia de Levenshtein
- Detección de anagramas
- Recomendaciones basadas en historial

Incluye funcionalidades como historial de comandos, autocompletado con `readline`, colores ANSI y manejo de señales.

## Características principales
- **Corrección de errores tipográficos** en comandos mal escritos.
- **Historial de comandos** con navegación mediante flechas.
- **Sugerencias inteligentes** de comandos similares.
- **Soporte para señales** (`SIGINT`, `SIGTERM`) para manejo seguro.
- **Interfaz en colores** para mejorar la experiencia de usuario.

## Instalación
Para compilar e instalar DWIMSH, ejecute:
```sh
gcc -o dwimsh dwimsh.c -lreadline -lm
```
Asegúrese de tener `readline` instalado. En Ubuntu/Debian:
```sh
sudo apt install libreadline-dev
```

## Uso
Ejecute el shell con:
```sh
./dwimsh
```
Dentro del shell, use los siguientes comandos:
- `help` → Muestra ayuda
- `list` → Lista los comandos disponibles
- `history` → Muestra el historial de comandos
- `exit` → Salir del shell

Si un comando no existe, DWIMSH sugiere posibles correcciones.

## Ejemplo de uso
```sh
$ lss
Comando no encontrado: lss
¿Quisiste decir "ls"? [y/n]
```
Si se responde `y`, ejecutará `ls`.

## Contribuciones
Las contribuciones son bienvenidas. Para reportar errores o sugerir mejoras, envíe un *pull request* o abra un *issue*.

## Autor
Escrito por **Víctor Romero - 12211079**
