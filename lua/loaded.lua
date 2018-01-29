cl_env = {}
for k,v in pairs(getfenv(0)) do
    cl_env[k] = v
end
cl_env.hook = use "hook.lua"
for _, k in pairs { "player" } do
    cl_env[k] = cl_g[k]
end


local function init()
    
    RegisterHookCall(hook.Call)
    hook.Add("HUDPaint", "test_esp", function()
        for k,v in pairs(player.GetAll()) do
            if (v:IsDormant()) then
                continue
            end
            local pos = v:GetPos():ToScreen()
            if (not pos.visible) then
                continue
            end
            draw.SimpleTextOutlined(v:Nick(), "DermaDefault", pos.x, pos.y, nil, TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER, 1, Color(0,0,0,255))
        end
    end)
end
setfenv(init, cl_env)
init()
