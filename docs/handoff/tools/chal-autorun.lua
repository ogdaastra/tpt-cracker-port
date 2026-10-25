-- plays the reference solution of every challenge (or those in $IDS) and prints the outcome
dofile("/home/user/tpt-cracker-port/src/lua/luascripts/challenges.lua")
tpt.setfpscap(2)
if os.getenv("EXTRA") then dofile(os.getenv("EXTRA")) end
local ids = {}
if os.getenv("IDS") and os.getenv("IDS") ~= "" then
	for id in os.getenv("IDS"):gmatch("[^,]+") do ids[#ids + 1] = id end
else
	for _, d in ipairs(challenges.list()) do ids[#ids + 1] = d.id end
end
local mode = os.getenv("MODE") or "solve" -- solve | idle (do nothing: must not win)
local verbose = os.getenv("VERBOSE") == "1"
local k, wait, started = 0, 0, false
local t0 = os.clock()
local function report(st, res)
	print(string.format("RESULT %s %s %s frames=%d progress=%.2f stars=%s ink=%d solink=%s", mode, ids[k], tostring(res),
		st and st.frame or -1, st and st.progress or -1, tostring(st and st.stars), st and st.ink or -1, tostring(st and st.solutionInk)))
	io.stdout:flush()
end
event.register(event.tick, function()
	if wait > 0 then wait = wait - 1 return end
	local ok, st = pcall(challenges.state)
	if started and ok and st and not st.result then
		if verbose and st.frame % 120 == 0 and st.frame ~= lastFrame then
			lastFrame = st.frame
			local extra = ""
			if os.getenv("PROBE") then
				local f = load("local C = challenges.C return " .. os.getenv("PROBE"))
				local okp, v = pcall(f)
				extra = tostring(v)
			end
			print(string.format("  %s frame=%d progress=%.3f %s", ids[k], st.frame, st.progress, extra)) io.stdout:flush()
		end
		return
	end
	if started then report(st, st and st.result) end
	k = k + 1
	if k > #ids then print("DONE " .. string.format("%.1f", os.clock() - t0)) io.stdout:flush() os.exit(0) end
	local ok2, err = pcall(function()
		if mode == "solve" then challenges.solve(ids[k]) else challenges.start(ids[k]) tpt.set_pause(0) end
	end)
	if not ok2 then
		print("RESULT " .. mode .. " " .. ids[k] .. " ERROR " .. tostring(err)) io.stdout:flush()
		started = false
	else
		started = true
	end
	wait = 2
end)
