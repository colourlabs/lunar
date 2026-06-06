local Router = require("lunar/router")
local app = Router.new()

app:get("/", function()
    return { status = 200, body = "hello from router" }
end)

app:get("/hello", function(req)
    return {
        status = 200,
        body   = "hello, " .. (req.m_query and req.m_query.name or "world")
    }
end)

function handle(req)
    return app:dispatch(req)
end