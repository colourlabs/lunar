# lunar

A lightweight HTTP workers runtime for Lua built on asynchronous I/O.

Write request handlers in Lua and serve them using a high-performance event loop powered by [**libuv**](https://libuv.org).

```lua
function handle(req)
    return {
        status = 200,
        headers = {
            ["Content-Type"] = "text/plain"
        },
        body = "Hello, " .. (req.query.name or "world")
    }
end
```

## features

* async runtime powered by [libuv](https://libuv.org).
* a secure sandbox with isolate architecture, each LuaState has it's own memory allocator and restricted API calls
* fast secure HTTP parsing with [llhttp](https://github.com/nodejs/llhttp).
* `lunar/json` API that uses [yyjson](https://github.com/ibireme/yyjson) under the hood for extreme performance.
* simple request/response API.
* minimal dependencies

## quick start

create `hello.lua`:

```lua
function handle(req)
    return {
        status = 200,
        headers = {
            ["Content-Type"] = "text/plain"
        },
        body = "Hello from Lunar!"
    }
end
```

run it:

```sh
./lunar hello.lua
```

then visit:

```text
http://localhost:8080
```

## request object

```lua
function handle(req)
    print(req.method)
    print(req.path)
    print(req.query.name)
end
```

## response object

handlers return a Lua table describing the HTTP response:

```lua
return {
    status = 200,
    headers = {
        ["Content-Type"] = "application/json"
    },
    body = '{"ok":true}'
}
```

## technology

lunar is built on:

* libuv for asynchronous networking and the event loop
* llhttp for HTTP/1.1 request parsing
* lua 5.5 for application logic (forked to add sandbox limits and possibly performance improvements over stock 5.5)

## building from source

```sh
git clone https://github.com/colourlabs/lunar
cd lunar

meson setup build
meson compile -C build
```

run a Lua worker:

```sh
./build/lunar hello.lua
```

## status

lunar is currently under active development. APIs may change between releases.
