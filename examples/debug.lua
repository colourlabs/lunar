function handle(req)
    local ok, err = pcall(function()
        local r = require("lunar/router")
    end)
    return {
        status = 200,
        body = ok and "ok" or tostring(err)
    }
end