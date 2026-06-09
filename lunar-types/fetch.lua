---@meta lunar.fetch

local fetch = {}

---@class FetchResponse
---@field status integer
---@field ok boolean
---@field body string
---@field headers table<string, string>
local FetchResponse = {}

---@return any
function FetchResponse:json() end

---@class FetchOptions
---@field method? string
---@field body? string
---@field timeout? integer
---@field follow_redirects? boolean
---@field headers? table<string, string>

---@param url string
---@param options? FetchOptions
---@return FetchResponse
function fetch(url, options) end

return fetch
