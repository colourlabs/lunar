local methods = { "GET", "POST", "PUT", "DELETE", "PATCH" }

local Router = {}
Router.__index = Router

function Router.new()
    return setmetatable({
        exact    = {},
        dynamic  = {},
        on_404   = nil,
        on_405   = nil,
        on_error = nil,
    }, Router)
end

for _, method in ipairs(methods) do
    Router[method:lower()] = function(self, path, handler)
        if path:find(":") then
            self.dynamic[method] = self.dynamic[method] or {}

            local param_names = {}
            for name in path:gmatch(":([%w_]+)") do
                table.insert(param_names, name)
            end

            local pattern = "^" .. path:gsub(":[%w_]+", "([^/]+)") .. "$"

            table.insert(self.dynamic[method], {
                pattern     = pattern,
                handler     = handler,
                param_names = param_names,
            })
        else
            self.exact[method] = self.exact[method] or {}
            self.exact[method][path] = handler
        end
    end
end

function Router:not_found(handler)
    self.on_404 = handler
end

function Router:method_not_allowed(handler)
    self.on_405 = handler
end

function Router:error(handler)
    self.on_error = handler
end

local function match_dynamic(routes, path)
    for _, route in ipairs(routes) do
        local matches = { string.match(path, route.pattern) }
        if #matches > 0 then
            local params = {}
            for i, name in ipairs(route.param_names) do
                params[name] = matches[i]
            end
            return route.handler, params
        end
    end
    return nil, nil
end

local function default_404(_req)
    return {
        status  = 404,
        headers = { ["Content-Type"] = "text/plain" },
        body    = "not found",
    }
end

local function default_405(_req)
    return {
        status  = 405,
        headers = { ["Content-Type"] = "text/plain" },
        body    = "method not allowed",
    }
end

local function default_error(_req, _err)
    return {
        status  = 500,
        headers = { ["Content-Type"] = "text/plain" },
        body    = "internal server error",
    }
end

function Router:dispatch(req)
    local method = req.method
    local path   = req.path

    if #path > 1 and path:sub(-1) == "/" then
        path = path:sub(1, -2)
    end

    local handler = nil

    if self.exact[method] and self.exact[method][path] then
        handler = self.exact[method][path]
    end

    if not handler and self.dynamic[method] then
        local matched_handler, params = match_dynamic(self.dynamic[method], path)
        if matched_handler then
            req.params = params
            handler = matched_handler
        end
    end

    if not handler then
        for _, m in ipairs(methods) do
            if m ~= method then
                if self.exact[m] and self.exact[m][path] then
                    return (self.on_405 or default_405)(req)
                end
                if self.dynamic[m] then
                    local h = match_dynamic(self.dynamic[m], path)
                    if h then
                        return (self.on_405 or default_405)(req)
                    end
                end
            end
        end
        return (self.on_404 or default_404)(req)
    end

    local ok, result = pcall(handler, req)

    if not ok then
        return (self.on_error or default_error)(req, result)
    end

    if type(result) ~= "table" then
        return (self.on_error or default_error)(req,
            "handler must return a table, got " .. type(result))
    end

    if type(result.status) ~= "number" then
        return (self.on_error or default_error)(req,
            "handler response must have a numeric status")
    end

    return result
end

return Router