-- Hello World Mod - Ejemplo de mod para CTR-PC-Port
-- Este mod muestra cómo crear un mod básico con Lua

-- API global del mod (disponible automáticamente)
local mod = {}

-- Función de inicialización - se ejecuta cuando el mod se carga
mod.hook("onInit", function()
    mod.log("¡Hola Mundo! El mod se ha cargado correctamente.")
    mod.log("Este es un ejemplo básico de mod para CTR-PC-Port.")
    mod.log("Puedes ver este mensaje en la pantalla de gestión de mods.")
end)

-- Función de actualización - se ejecuta en cada fotograma
mod.hook("onUpdate", function(dt)
    -- dt = delta time desde el último fotograma
    mod.log("Actualización del mod - FPS: " .. (1.0 / dt))
end)

-- Función de entrada - se ejecuta cuando se presiona una tecla
mod.hook("onInput", function(player, input)
    if input == "BTN_CROSS" then
        mod.log("¡Cruz presionado! Has pulsado el botón de acción.")
    elseif input == "BTN_TRIANGLE" then
        mod.log("¡Triángulo presionado! Has pulsado el botón de volver.")
    end
end)

-- Función de renderizado - se ejecuta en cada fotograma
mod.hook("onRender", function()
    mod.log("Renderizando el mod...")
end)

-- Función de inicialización de título
mod.hook("onTitleInit", function()
    mod.log("¡Inicializando título! El mod está listo para la pantalla de título.")
end)

-- Función para leer un archivo
mod.hook("onFileOpen", function(path, mode)
    if path == "hello.txt" then
        local content = mod.readFile("hello.txt")
        if content then
            mod.log("Contenido del archivo hello.txt: " .. content)
        end
    end
end)

-- Función principal - se ejecuta al inicio
function _G.main()
    mod.log("=== MODO HOLA MUNDO ===")
    mod.log("Este mod demuestra las funcionalidades básicas.")
    mod.log("")
    mod.log("Características:")
    mod.log("✓ Hook de inicialización (onInit)")
    mod.log("✓ Hook de actualización (onUpdate)")
    mod.log("✓ Hook de entrada (onInput)")
    mod.log("✓ Hook de renderizado (onRender)")
    mod.log("✓ Hook de título (onTitleInit)")
    mod.log("✓ Lectura de archivos (readFile)")
    mod.log("✓ Escritura de archivos (writeFile)")
    mod.log("✓ Sistema de archivos (VFS)")
    mod.log("")
    mod.log("Presiona TRIÁNGULO para volver al menú principal.")
end

-- Ejecutar función principal
_G.main()