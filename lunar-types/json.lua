---@meta lunar.json

local json = {}

---@param str string
---@return any
function json.decode(str) end

---@param value any
---@return string
function json.encode(value) end

json.empty_object = nil
json.empty_array = nil
json.array_mt = {}

return json
