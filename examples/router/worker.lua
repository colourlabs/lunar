local router = require("lunar/router")
local json = require("lunar/json")

local app = router.new()

app:get("/", function()
    return { status = 200, body = "hello from router" }
end)

app:post("/", function(req)
    if req.body == nil or req.body == "" then
        return {
            status  = 400,
            headers = { ["Content-Type"] = "application/json" },
            body    = json.encode({ ok = false, error = "empty body" }),
        }
    end

    local ok, data = pcall(json.decode, req.body)
    if not ok then
        return {
            status  = 400,
            headers = { ["Content-Type"] = "application/json" },
            body    = json.encode({ ok = false, error = data }),
        }
    end

    return {
        status  = 200,
        headers = { ["Content-Type"] = "application/json" },
        body    = json.encode({ ok = true, echo = data }),
    }
end)

app:get("/hello", function(req)
    return {
        status = 200,
        body = "hello, " .. (req.query and req.query.name or "world")
    }
end)

function handle(req)
    return app:dispatch(req)
end