-- TODO: implement

local Router = {}
Router.__index = Router

-- creates a new instance with empty routes
function Router.new()
    return setmetatable({ routes = {} }, Router)
end

function Router:get(path, handler)
    table.insert(self.routes, { method = "GET", path = path, handler = handler })
end

function Router:post(path, handler)
    table.insert(self.routes, { method = "POST", path = path, handler = handler })
end

function Router:put(path, handler)
    table.insert(self.routes, { method = "PUT", path = path, handler = handler })
end

function Router:delete(path, handler)
    table.insert(self.routes, { method = "DELETE", path = path, handler = handler })
end

function Router:patch(path, handler)
    table.insert(self.routes, { method = "PATCH", path = path, handler = handler })
end

function Router:dispatch(req)
    for _, route in ipairs(self.routes) do
        if route.method == req.method then
            return route.handler(req)
        end
    end

    return { status = 404, body = "not found" }
end

return Router