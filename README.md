# lunar

A lightweight async workers runtime for Lua 5.5 - write request handlers in Lua, run them with async I/O

```lua
function handle(req)
    return {
        status  = 200,
        headers = { ["Content-Type"] = "text/plain" },
        body    = "Hello, " .. (req.query.name or "world"),
    }
end
```
