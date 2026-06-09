---@meta lunar.router

local Router = {}

---@class Request
---@field method string
---@field path string
---@field headers table<string, string>
---@field query table<string, string>
---@field body string
---@field params? table<string, string>

---@class Response
---@field status integer
---@field headers? table<string, string>
---@field body? string

---@return table
function Router.new() end
---@param path string
---@param handler fun(req: Request): Response
function Router:get(path, handler) end
---@param path string
---@param handler fun(req: Request): Response
function Router:post(path, handler) end
---@param path string
---@param handler fun(req: Request): Response
function Router:put(path, handler) end
---@param path string
---@param handler fun(req: Request): Response
function Router:delete(path, handler) end
---@param path string
---@param handler fun(req: Request): Response
function Router:patch(path, handler) end
---@param handler fun(req: Request): Response
function Router:not_found(handler) end
function Router:method_not_allowed(handler) end
function Router:error(handler) end
---@param req Request
---@return Response
function Router:dispatch(req) end

return Router
