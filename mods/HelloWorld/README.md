# Modo Hello World - Ejemplo de Mod para CTR-PC-Port

## Descripción

Este es un mod de ejemplo básico que demuestra todas las funcionalidades principales del sistema de mods.

## Cómo funciona

1. **Estructura del mod**
   ```
   mods/
   └── HelloWorld/
       ├── main.lua        # Código principal del mod
       └── hello.txt      # Archivo de ejemplo
   ```

2. **Características principales**
   - Hook de inicialización (onInit)
   - Hook de actualización (onUpdate)
   - Hook de entrada (onInput)
   - Hook de renderizado (onRender)
   - Hook de título (onTitleInit)
   - Lectura/escritura de archivos
   - Sistema de archivos VFS

3. **Cómo probarlo**
   - Copia este mod a `mods/HelloWorld/`
   - Inicia el juego
   - Ve al menú principal
   - Selecciona "MODS"
   - Observa los mensajes en la consola

## API del Mod

El mod proporciona las siguientes funciones globales:

### `mod.log(mensaje)`
Imprime un mensaje en la consola.

### `mod.hook(evento, funcion)`
Registra una función para un evento específico.

### `mod.readFile(ruta)`
Lee el contenido de un archivo desde la carpeta del mod.

### `mod.writeFile(ruta, contenido)`
Escribe contenido en un archivo en la carpeta del mod.

### `mod.getModPath()`
Obtiene la ruta absoluta del directorio del mod.

## Eventos Disponibles

- `onInit` - Se ejecuta cuando el mod se carga
- `onUpdate(dt)` - Se ejecuta en cada fotograma (dt = delta time)
- `onInput(player, input)` - Se ejecuta cuando se presiona un botón
- `onRender()` - Se ejecuta en cada fotograma para renderizado
- `onTitleInit()` - Se ejecuta al inicializar la pantalla de título

## Ejemplo de uso

```lua
-- main.lua
mod.hook("onInit", function()
    mod.log("¡Hola Mundo! El mod se ha cargado.")
end)

mod.hook("onInput", function(player, input)
    if input == "BTN_CROSS" then
        mod.log("¡Cruz presionado!")
    end
end)
```

## Sistema de Archivos

El mod puede leer y escribir archivos en su propia carpeta:
- `hello.txt` - Archivo de ejemplo
- Cualquier archivo que crees puede ser leído por el mod

## Reemplazo de Archivos

El mod puede reemplazar archivos del juego:
- Los archivos en `mods/HelloWorld/files/` reemplazan a los del juego
- Por ejemplo: `mods/HelloWorld/files/TEST.STR` reemplaza `assets/TEST.STR`

## Notas

- Los logs del mod aparecen en la consola
- El sistema de archivos es relativo a la carpeta del mod
- Los hooks pueden ser registrados múltiples veces
- Los mods se cargan al inicio del juego

## Créditos

Basado en el sistema de mods de CTR-PC-Port.
