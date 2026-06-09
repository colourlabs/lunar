-- selene: allow(unused_variable)
function handle(req)
	-- these should all fail
	local ok, err

	ok, err = pcall(function()
		io.open("secret.txt")
	end)
	if ok then
		return { status = 500, body = "ESCAPE: io worked" }
	end

	ok, err = pcall(function()
		os.exit()
	end)
	if ok then
		return { status = 500, body = "ESCAPE: os worked" }
	end

	ok, err = pcall(function()
		require("os")
	end)
	if ok then
		return { status = 500, body = "ESCAPE: require os worked" }
	end

	ok, err = pcall(function()
		load("return 1")()
	end)
	if ok then
		return { status = 500, body = "ESCAPE: load worked" }
	end

	return {
		status = 200,
		body = "sandbox holding",
	}
end
