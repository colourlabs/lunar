function handle(req)
    return {
        status = 200,
        headers = { ["Content-Type"] = "text/plain" },
        body = "Hello, " .. (req.query.name or "world"),
    }
end