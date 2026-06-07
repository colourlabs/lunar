local fetch = require("lunar/fetch")
local json = require("lunar/json")
local router = require("lunar/router")

local app = router.new()

app:get("/proxy", function(req)
    local res = fetch("https://jsonplaceholder.typicode.com/todos/1")
    if not res.ok then
        return { status = 502, body = "upstream error" }
    end
    return {
        status  = 200,
        headers = { ["Content-Type"] = "application/json" },
        body    = res.body,
    }
end)

-- or decode inline
app:get("/proxy2", function(req)
    local res = fetch("https://jsonplaceholder.typicode.com/todos/1")
    local data = res:json()
    return {
        status = 200,
        headers = { ["Content-Type"] = "application/json" },
        body = json.encode({ title = data.title }),
    }
end)

function handle(req)
    return app:dispatch(req)
end