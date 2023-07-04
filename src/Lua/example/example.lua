-- Create a Font for later use
font = gui.Font()

-- Add a .zip with resource to be loaded by the game
loader.addPackage(loader.basePath.."/example.zip")

-- Listen for Scene change event
soku.SubscribeSceneChange(function(id, scene)
    if id ~= 2 then return end -- if not title then skip
    textSprite = scene.renderer:createText("Hello World! This is a example.", font, 400, 20); -- create a text
    return true -- enable changes on this scene
end)
