-- PS1 framebuffer: 512 x 216
-- Center: x=256, y=108

mod.hook("onInit", function()
    mod.log("Hello World mod loaded!")
end)

mod.hook("onRender", function()
    mod.drawText("Hello World", 256, 108, 2, 1)
end)
