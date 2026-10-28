-- Challenge mode for the web port of Cracker1000's Mod (GPL-3.0, see LICENSE).
-- Each challenge builds a scene, limits which tools the player may use (challenge.begin, implemented in
-- C++), checks a goal after every simulation frame and awards up to three stars. Every challenge also
-- has a reference solution, which the browser tests play through to prove it can be won.
-- Interface: challenges.command("start <id>" | "stop" | "restart" | "hint" | "export"), an in-game
-- window (button in the second side-bar column) and challenge.notify messages for the web page.

-- loading the file again (while developing challenges) replaces the running copy
if _G.challenges and _G.challenges.unload then _G.challenges.unload() end
local challenges = {}
_G.challenges = challenges

local list, byId = {}, {}
local current = nil -- running challenge state
local PROGRESS_FILE = "challenges-progress.json"
local progress = {} -- id -> { stars = n, frames = n, ink = n }

local CATEGORIES = {
	{ id = "heat", name = "Heat" },
	{ id = "electronics", name = "Electronics" },
	{ id = "explosives", name = "Explosives" },
	{ id = "chemistry", name = "Chemistry and liquids" },
	{ id = "machines", name = "Machines and force" },
	{ id = "gravity", name = "Gravity and space" },
	{ id = "life", name = "Life and nature" },
	{ id = "characters", name = "Characters" },
	{ id = "puzzles", name = "Puzzles and records" },
}

local WALLS = {
	ERASE = 0, CNDTW = 1, EWALL = 2, DTECT = 3, STRM = 4, FAN = 5, LIQD = 6, ABSRB = 7, WALL = 8, AIR = 9,
	POWDR = 10, CNDTR = 11, EHOLE = 12, GAS = 13, GRVTY = 14, ENRGY = 15, NOAIR = 16, ERASEA = 17,
	STASIS = 18, ZHOLE = 19, INDESTRUCTIBLE = 20, FARADAY = 21, ONEWAY = 22,
}

-- ---------------------------------------------------------------- helpers

local function E(name)
	local id = elem["DEFAULT_PT_" .. name]
	if not id then
		local ok, byName = pcall(tpt.element, name)
		id = ok and byName or nil
	end
	if not id or id <= 0 then error("challenges: unknown element " .. tostring(name)) end
	return id
end

-- tool identifiers from short names: "WATR" (element), "WALL:WALL" (wall), "TOOL:HEAT" (tool)
local function toolId(name)
	local kind, rest = name:match("^(%u+):(.+)$")
	if kind == "WALL" then return "DEFAULT_WL_" .. rest end
	if kind == "TOOL" then return "DEFAULT_TOOL_" .. rest end
	if kind == "LIFE" then return "DEFAULT_PT_LIFE_" .. rest end
	return elem.property(E(name), "Identifier")
end

local function json(v)
	local t = type(v)
	if t == "table" then
		if #v > 0 or next(v) == nil then
			local out = {}
			for i = 1, #v do out[i] = json(v[i]) end
			return "[" .. table.concat(out, ",") .. "]"
		end
		local out = {}
		for k, val in pairs(v) do out[#out + 1] = json(tostring(k)) .. ":" .. json(val) end
		return "{" .. table.concat(out, ",") .. "}"
	elseif t == "string" then
		return '"' .. v:gsub('[%c"\\]', function(c) return string.format("\\u%04x", c:byte()) end) .. '"'
	elseif t == "number" then
		return string.format("%.6g", v)
	elseif t == "boolean" then
		return tostring(v)
	end
	return "null"
end

-- minimal reader for the progress file written by json() above: {"id":{"stars":n,...},...}
local function readProgress()
	local f = io.open(PROGRESS_FILE, "r")
	if not f then return {} end
	local text = f:read("*a")
	f:close()
	local out = {}
	for id, body in text:gmatch('"([%w%-]+)":{([^}]*)}') do
		local entry = {}
		for k, num in body:gmatch('"(%w+)":(%-?[%d%.e%+]+)') do entry[k] = tonumber(num) end
		out[id] = entry
	end
	return out
end

local function writeProgress()
	local f = io.open(PROGRESS_FILE, "w")
	if f then
		f:write(json(progress))
		f:close()
	end
end

-- Scene building (used by setup and solution functions)
local S = {}
function S.box(x0, y0, x1, y1, el, props)
	local t = type(el) == "number" and el or E(el)
	for x = x0, x1 do for y = y0, y1 do
		local i = sim.partCreate(-1, x, y, t)
		if i >= 0 and props then for k, v in pairs(props) do sim.partProperty(i, k, v) end end
	end end
end
-- like S.part, but replaces whatever is there
function S.put(x, y, el, props)
	local old = sim.partID(x, y)
	if old then sim.partKill(old) end
	return S.part(x, y, el, props)
end
function S.putBox(x0, y0, x1, y1, el, props)
	for x = x0, x1 do for y = y0, y1 do S.put(x, y, el, props) end end
end
function S.part(x, y, el, props)
	local i = sim.partCreate(-1, x, y, type(el) == "number" and el or E(el))
	if i >= 0 and props then for k, v in pairs(props) do sim.partProperty(i, k, v) end end
	return i
end
function S.walls(x0, y0, x1, y1, wall)
	sim.createWallBox(x0, y0, x1, y1, WALLS[wall or "WALL"])
end
function S.lock(x0, y0, x1, y1)
	current.locked[#current.locked + 1] = { x0, y0, x1, y1 }
end
-- a container: sides and bottom of the given element around an inside rectangle, locked unless open
function S.tank(x0, y0, x1, y1, el, thick, unlocked)
	thick = thick or 2
	local parts = {
		{ x0 - thick, y0, x0 - 1, y1 + thick }, { x1 + 1, y0, x1 + thick, y1 + thick }, { x0, y1 + 1, x1, y1 + thick },
	}
	for _, r in ipairs(parts) do
		S.box(r[1], r[2], r[3], r[4], el)
		if not unlocked then S.lock(r[1], r[2], r[3], r[4]) end
	end
end
-- the ground: a wall floor along the bottom of the simulation (particles rest on y = top - 1)
function S.floor(top)
	S.walls(0, top or 372, sim.XRES - 1, sim.YRES - 1, "WALL")
end
-- a closed wall box (outline only), one cell thick, around the given pixel rectangle
function S.wallBox(x0, y0, x1, y1, wall)
	S.walls(x0 - 4, y0 - 4, x1 + 4, y0 - 1, wall)
	S.walls(x0 - 4, y1 + 1, x1 + 4, y1 + 4, wall)
	S.walls(x0 - 4, y0, x0 - 1, y1, wall)
	S.walls(x1 + 1, y0, x1 + 4, y1, wall)
end
function S.temp(x0, y0, x1, y1, kelvin, el)
	local t = el and E(el)
	for i in sim.parts() do
		local x, y = sim.partPosition(i)
		if x >= x0 and x <= x1 and y >= y0 and y <= y1 and (not t or sim.partProperty(i, "type") == t) then
			sim.partProperty(i, "temp", kelvin)
		end
	end
end

-- Goal checks (used by check functions)
local C = {}
-- particles of a type (nil: any) in a region (default: everywhere), optionally also passing test(i)
function C.count(el, x0, y0, x1, y1, test)
	local t = el and E(el)
	local n = 0
	x0, y0, x1, y1 = x0 or 0, y0 or 0, x1 or sim.XRES, y1 or sim.YRES
	for i in sim.parts() do
		if not t or sim.partProperty(i, "type") == t then
			local x, y = sim.partPosition(i)
			if x >= x0 and x <= x1 and y >= y0 and y <= y1 and (not test or test(i)) then n = n + 1 end
		end
	end
	return n
end
function C.total(el)
	return C.count(el)
end
-- the highest and the average air pressure over the cells of a region
function C.pressureIn(x0, y0, x1, y1)
	local max, sum, n = -1e9, 0, 0
	for cy = math.floor(y0 / 4), math.floor(y1 / 4) do
		for cx = math.floor(x0 / 4), math.floor(x1 / 4) do
			local p = sim.pressure(cx, cy)
			if p > max then max = p end
			sum = sum + p
			n = n + 1
		end
	end
	return max, sum / n
end
function C.avgTemp(el, x0, y0, x1, y1)
	local t = el and E(el)
	local n, sum = 0, 0
	for i in sim.parts() do
		if not t or sim.partProperty(i, "type") == t then
			local x, y = sim.partPosition(i)
			if x >= x0 and x <= x1 and y >= y0 and y <= y1 then
				n = n + 1
				sum = sum + sim.partProperty(i, "temp")
			end
		end
	end
	return n > 0 and sum / n or nil, n
end
function C.maxTemp()
	local m = -1
	for i in sim.parts() do
		local tmp = sim.partProperty(i, "temp")
		if tmp > m then m = tmp end
	end
	return m
end
function C.pressure(x, y)
	return sim.pressure(math.floor(x / 4), math.floor(y / 4))
end
-- any particle of the given type with life > 0 in the region (e.g. a lit LED)
function C.lit(el, x0, y0, x1, y1)
	local t = E(el)
	for i in sim.parts() do
		if sim.partProperty(i, "type") == t and sim.partProperty(i, "life") > 0 then
			local x, y = sim.partPosition(i)
			if x >= x0 and x <= x1 and y >= y0 and y <= y1 then return true end
		end
	end
	return false
end
local function clamp01(v) return v < 0 and 0 or (v > 1 and 1 or v) end
C.clamp01 = clamp01

-- ---------------------------------------------------------------- definitions

-- Definition fields: id, cat, diff (1-5), title, text, tools, ink (particle budget), time (seconds),
-- setup(S, st, C), check(C, st) -> progress 0..1 [, failure reason], solution(S, st) (may set st.steps =
-- { { frame, function(S, st) } }), optional each(S, st) run every frame (hazards, heaters), survive =
-- seconds (won when check stays satisfied that long), hold (frames the goal must stay reached),
-- stars = { time = { 3-star s, 2-star s }, ink = { 3-star, 2-star } }, hints.
local function add(def)
	assert(def.id and not byId[def.id], "challenges: duplicate or missing id " .. tostring(def.id))
	if def.survive then
		def.time = def.time or (def.survive + 5)
		local inner = def.check
		def.check = function(C, st)
			local ok, failure = inner(C, st)
			if failure then return 0, failure end
			if not ok or ok < 1 then return 0, def.failText or "failed" end
			return st.frame / (def.survive * 60)
		end
	end
	def.time = def.time or 60
	def.hold = def.hold or 1
	def.checkEvery = def.checkEvery or 10
	list[#list + 1] = def
	byId[def.id] = def
end
challenges.add = add
challenges.S, challenges.C = S, C

-- ---------------------------------------------------------------- running

local function notify(t)
	if challenge and challenge.notify then challenge.notify(json(t)) end
end

local function stars(def, frames, ink)
	local s = def.stars or {}
	local secs = frames / 60
	local n = 1
	local t2, t3 = s.time and s.time[2], s.time and s.time[1]
	local i2, i3 = s.ink and s.ink[2], s.ink and s.ink[1]
	if (not t2 or secs <= t2) and (not i2 or ink <= i2) then n = 2 end
	if (not t3 or secs <= t3) and (not i3 or ink <= i3) then n = 3 end
	if s.custom then n = s.custom(frames, ink) end
	return n
end

local function finish(result)
	local def = current.def
	current.result = result
	current.resultFrame = current.frame
	current.finalInk = challenge.ink()
	if result == "won" then
		local ink = challenge.ink()
		local n = math.max(1, stars(def, current.frame, ink) - (current.hintsUsed or 0))
		current.stars = n
		local best = progress[def.id]
		-- a run of the reference solution proves the challenge works but earns nothing
		if not current.solved and (not best or n > (best.stars or 0) or (n == best.stars and current.frame < (best.frames or 1e9))) then
			progress[def.id] = { stars = n, frames = current.frame, ink = ink }
			writeProgress()
		end
		notify({ type = "won", id = def.id, stars = n, frames = current.frame, ink = ink, solved = current.solved or false })
	else
		notify({ type = "lost", id = def.id, reason = result })
	end
	challenge.finish() -- the scene stays, the tools are free again
end

local function onAfterSim()
	if not current or current.result then return end
	local def = current.def
	current.frame = current.frame + 1
	if current.steps then
		for _, step in ipairs(current.steps) do
			if step[1] == current.frame then step[2](S, current) end
		end
	end
	if def.each then def.each(S, current) end
	if current.frame % def.checkEvery == 0 then
		local ok, p, failed = pcall(def.check, C, current)
		if not ok then
			print("challenge " .. def.id .. " check error: " .. tostring(p))
			p = 0
		end
		current.progress = clamp01(p or 0)
		if failed then
			finish(failed)
			return
		end
		if current.progress >= 1 then
			current.held = current.held + def.checkEvery
			if current.held >= def.hold then
				finish("won")
				return
			end
		else
			current.held = 0
		end
		if current.frame % 30 == 0 then
			notify({ type = "progress", id = def.id, progress = current.progress, frames = current.frame, ink = challenge.ink() })
		end
	end
	if current.frame >= def.time * 60 then
		finish("time")
	end
end

function challenges.start(id)
	local def = byId[id]
	if not def then error("challenges: no challenge " .. tostring(id)) end
	-- the challenges are built for the classic world (the page's World size setting can change it)
	if sim.XRES ~= 612 or sim.YRES ~= 384 then error("challenges: they need the classic 612x384 world") end
	if current and not current.result then challenge.finish() end
	if current and current.def.cleanup then current.def.cleanup() end
	sim.clearSim()
	tpt.set_pause(1)
	current = { def = def, frame = 0, held = 0, progress = 0, locked = {}, hintsUsed = 0 }
	if def.prepare then def.prepare() end
	def.setup(S, current, C)
	local tools = {}
	for _, name in ipairs(def.tools) do tools[#tools + 1] = toolId(name) end
	challenge.begin({ tools = tools, ink = def.ink, locked = current.locked })
	notify({ type = "started", id = id })
	return current
end

function challenges.stop()
	if current and not current.result then challenge.finish() end
	if current and current.def.cleanup then current.def.cleanup() end
	current = nil
	notify({ type = "stopped" })
end

-- reference solution, used by the tests: builds the answer with Lua (not limited by the challenge rules)
function challenges.solve(id)
	local state = challenges.start(id or (current and current.def.id))
	state.solved = true
	local before = C.count()
	state.def.solution(S, state)
	state.solutionInk = C.count() - before
	tpt.set_pause(0)
	return state
end

function challenges.state()
	if not current then return nil end
	return {
		id = current.def.id, frame = current.frame, progress = current.progress, result = current.result,
		stars = current.stars, ink = current.finalInk or challenge.ink(), solutionInk = current.solutionInk,
	}
end

function challenges.export()
	local out = {}
	for _, def in ipairs(list) do
		local tools = {}
		for i, name in ipairs(def.tools) do tools[i] = name end
		out[#out + 1] = {
			id = def.id, title = def.title, category = def.cat, difficulty = def.diff, text = def.text,
			hints = def.hints or {}, tools = tools, ink = def.ink or -1, time = def.time,
		}
	end
	notify({ type = "catalog", categories = CATEGORIES, challenges = out, progress = progress })
	return #out
end

function challenges.hint()
	if not current or current.result then return end
	local hints = current.def.hints or {}
	current.hintsUsed = math.min((current.hintsUsed or 0) + 1, #hints)
	notify({ type = "hint", id = current.def.id, hint = hints[current.hintsUsed], used = current.hintsUsed })
	return hints[current.hintsUsed]
end

function challenges.command(text)
	local cmd, arg = text:match("^(%S+)%s*(.*)$")
	if cmd == "start" then challenges.start(arg)
	elseif cmd == "solve" then challenges.solve(arg)
	elseif cmd == "restart" then if current then challenges.start(current.def.id) end
	elseif cmd == "stop" then challenges.stop()
	elseif cmd == "hint" then challenges.hint()
	elseif cmd == "export" then challenges.export()
	end
end

function challenges.list() return list end
function challenges.categories() return CATEGORIES end
function challenges.progress() return progress end

-- ---------------------------------------------------------------- in-game display

local function drawHud()
	if not current then return end
	local def = current.def
	local w = 300
	local x, y = math.floor((sim.XRES - w) / 2), 20
	gfx.fillRect(x, y, w, 40, 0, 0, 0, 170)
	gfx.drawRect(x, y, w, 40, 255, 216, 32, 200)
	gfx.drawText(x + 6, y + 4, def.title, 255, 216, 32, 255)
	local secs = math.floor(current.frame / 60)
	local right = string.format("%d:%02d / %d:%02d", math.floor(secs / 60), secs % 60, math.floor(def.time / 60), def.time % 60)
	if def.ink and def.ink >= 0 then right = "ink " .. (current.finalInk or challenge.ink()) .. "/" .. def.ink .. "   " .. right end
	gfx.drawText(x + w - 6 - gfx.textSize(right), y + 4, right, 200, 200, 200, 255)
	gfx.drawText(x + 6, y + 16, def.text, 220, 220, 220, 255)
	gfx.fillRect(x + 6, y + 30, w - 12, 5, 60, 60, 60, 255)
	gfx.fillRect(x + 6, y + 30, math.floor((w - 12) * (current.progress or 0)), 5, 80, 220, 120, 255)
	if current.result then
		local msg = current.result == "won" and string.rep("*", current.stars or 1) .. "  Challenge complete!" or
			(current.result == "time" and "Out of time - try again" or ("Failed: " .. current.result))
		local mw = gfx.textSize(msg) + 20
		gfx.fillRect(math.floor((sim.XRES - mw) / 2), 70, mw, 16, 0, 0, 0, 200)
		gfx.drawText(math.floor((sim.XRES - mw) / 2) + 10, 74, msg, current.result == "won" and 120 or 255,
			current.result == "won" and 255 or 120, 120, 255)
	end
end

-- the in-game challenge browser: categories on the left, challenges of the chosen category on the right
local function openWindow()
	local win = Window:new(-1, -1, 420, 290)
	local cat = 1
	local rows = {}
	local title = Label:new(10, 6, 400, 14, "Challenges")
	win:addComponent(title)
	local function fill()
		for _, r in ipairs(rows) do win:removeComponent(r) end
		rows = {}
		local yy = 26
		for _, def in ipairs(list) do
			if def.cat == CATEGORIES[cat].id then
				local best = progress[def.id]
				local label = string.rep("*", best and best.stars or 0) .. string.rep(".", 3 - (best and best.stars or 0)) ..
					"  " .. def.title .. "  (" .. string.rep("|", def.diff or 1) .. ")"
				local b = Button:new(140, yy, 270, 16, label, def.text)
				b:action(function() ui.closeWindow(win) challenges.start(def.id) end)
				win:addComponent(b)
				rows[#rows + 1] = b
				yy = yy + 18
				if yy > 260 then break end
			end
		end
	end
	for i, c in ipairs(CATEGORIES) do
		local b = Button:new(10, 26 + (i - 1) * 18, 120, 16, c.name)
		b:action(function() cat = i fill() end)
		win:addComponent(b)
	end
	local close = Button:new(10, 266, 120, 16, "Close")
	close:action(function() ui.closeWindow(win) end)
	win:addComponent(close)
	if current then
		local stop = Button:new(140, 266, 130, 16, "Stop challenge")
		stop:action(function() ui.closeWindow(win) challenges.stop() end)
		win:addComponent(stop)
		local again = Button:new(280, 266, 130, 16, "Restart")
		again:action(function() ui.closeWindow(win) challenges.start(current.def.id) end)
		win:addComponent(again)
	end
	win:onTryExit(function() ui.closeWindow(win) end)
	fill()
	ui.showWindow(win)
end
challenges.openWindow = openWindow

-- a side-bar button below the menus of the second column
local BX, BY = sim.XRES + 18, 392
local function drawButton()
	local hover = tpt.mousex >= BX and tpt.mousex < BX + 15 and tpt.mousey >= BY and tpt.mousey < BY + 15
	gfx.fillRect(BX, BY, 15, 15, hover and 70 or 0, hover and 60 or 0, 0, 255)
	gfx.drawRect(BX, BY, 15, 15, 255, 216, 32, 255)
	gfx.drawText(BX + 5, BY + 4, "*", 255, 216, 32, 255)
end

local function onTick()
	drawHud()
	drawButton()
end
local function onMouseDown(x, y, button)
	if x >= BX and x < BX + 15 and y >= BY and y < BY + 15 then
		openWindow()
		return false
	end
end
event.register(event.aftersim, onAfterSim)
event.register(event.tick, onTick)
event.register(event.mousedown, onMouseDown)
function challenges.unload()
	challenges.stop()
	event.unregister(event.aftersim, onAfterSim)
	event.unregister(event.tick, onTick)
	event.unregister(event.mousedown, onMouseDown)
end

progress = readProgress()

-- ---------------------------------------------------------------- the challenges
-- Coordinates: the simulation is 612x384; walls are 4x4 cells.

-- Most scenes stand on a wall floor at y = 372 (S.floor), so the lowest free row is y = 371.

local function life(i) return sim.partProperty(i, "life") end
-- for challenges that change simulation settings: set them in prepare, put the defaults back in cleanup
local function spaceOn() tpt.newtonian_gravity(1) sim.gravityMode(1) end
local function spaceOff() tpt.newtonian_gravity(0) sim.gravityMode(0) end

-- ===== Heat

add{
	id = "heat-kettle", cat = "heat", diff = 1, title = "Kettle",
	text = "Boil the water: turn 90% of it into steam.",
	tools = { "PLSM", "FIRE", "THRM", "COAL" }, ink = 400, time = 90,
	stars = { time = { 25, 50 }, ink = { 200, 320 } },
	hints = { "Titanium passes heat on to the water.", "Plasma is the hottest thing you have; put it under the kettle." },
	setup = function(S, st, C)
		S.floor()
		S.tank(256, 300, 355, 339, "TTAN")
		S.box(254, 342, 255, 371, "TTAN") S.lock(254, 342, 255, 371) -- legs
		S.box(356, 342, 357, 371, "TTAN") S.lock(356, 342, 357, 371)
		S.box(256, 315, 355, 339, "WATR")
		st.water = C.count("WATR")
	end,
	check = function(C, st) return (st.water - C.count("WATR")) / (st.water * 0.9) end,
	solution = function(S) S.box(256, 361, 355, 363, "PLSM") end,
}

add{
	id = "heat-deep-freeze", cat = "heat", diff = 1, title = "Deep freeze",
	text = "Freeze 90% of the water into ice.",
	tools = { "LNTG", "ICEI", "SNOW" }, ink = 1000, time = 60,
	stars = { time = { 15, 30 }, ink = { 600, 850 } },
	hints = { "Liquid nitrogen is very cold.", "Pour it right onto the water." },
	setup = function(S, st, C)
		S.floor()
		S.tank(256, 320, 355, 369, "GLAS")
		S.box(256, 350, 355, 369, "WATR")
		st.water = C.count("WATR")
	end,
	check = function(C, st) return C.count("ICEI") / (st.water * 0.9) end,
	solution = function(S) S.box(256, 300, 355, 307, "LNTG") end,
}

add{
	id = "heat-cool-down", cat = "heat", diff = 2, title = "Cool it",
	text = "Cool the red-hot iron block below 400 C.",
	tools = { "WATR", "ICEI", "SNOW", "LNTG" }, ink = 3000, time = 60,
	stars = { time = { 10, 25 }, ink = { 2000, 2700 } },
	hints = { "Water boils away and takes heat with it.", "Bury the block in ice." },
	setup = function(S)
		S.floor()
		S.box(296, 352, 315, 371, "IRON", { temp = 1173.15 })
		S.lock(296, 352, 315, 371)
	end,
	check = function(C)
		local t, n = C.avgTemp("IRON", 296, 352, 315, 371)
		if n < 300 then return 0, "the iron melted" end
		return (1173.15 - t) / (1173.15 - 673.15)
	end,
	solution = function(S) S.box(281, 280, 330, 329, "WATR") end,
}

add{
	id = "heat-glassworks", cat = "heat", diff = 3, title = "Glassworks",
	text = "Make 50 particles of glass from the sand.",
	tools = { "PLSM", "THRM", "FIRE", "WATR" }, ink = 1500, time = 60,
	stars = { ink = { 900, 1300 } },
	hints = { "Sand melts into molten glass, and that sets into glass when it cools.",
		"Thermite keeps it molten for too long. A short, very hot blast is better." },
	setup = function(S)
		S.floor()
		S.walls(248, 300, 255, 371) S.walls(356, 300, 363, 371)
		S.box(256, 362, 355, 371, "SAND")
		S.lock(256, 362, 355, 371)
	end,
	check = function(C) return C.count("GLAS") / 50 end,
	solution = function(S) S.box(256, 349, 355, 360, "PLSM") end,
}

add{
	id = "heat-relay", cat = "heat", diff = 2, title = "Heat relay",
	text = "The furnace is far away. Bring its heat to the tank and boil 80% of the water.",
	tools = { "HEAC", "METL", "TTAN", "PIPE" }, ink = 500, time = 120,
	stars = { time = { 40, 80 }, ink = { 250, 400 } },
	hints = { "HEAC passes heat very quickly.", "A line from the furnace to the tank is enough." },
	setup = function(S, st, C)
		S.floor()
		S.box(40, 330, 79, 371, "TTAN", { temp = 1673.15 }) -- below titanium's melting point
		S.lock(40, 330, 79, 371)
		S.tank(450, 330, 499, 369, "TTAN")
		S.box(450, 350, 499, 369, "WATR")
		st.water = C.count("WATR")
	end,
	each = function(S, st)
		if st.frame % 5 == 0 then S.temp(40, 330, 79, 371, 1673.15, "TTAN") end
	end,
	check = function(C, st) return (st.water - C.count("WATR")) / (st.water * 0.8) end,
	solution = function(S) S.box(80, 369, 447, 369, "HEAC") end,
}

-- ===== Electronics

add{
	id = "elec-first-light", cat = "electronics", diff = 1, title = "First light",
	text = "Light the LED.",
	tools = { "BTRY", "METL", "PSCN" }, ink = 150, time = 30,
	stars = { ink = { 40, 100 } },
	hints = { "A battery powers metal touching it.", "LEDs only light up from P-type silicon (PSCN)." },
	setup = function(S)
		S.box(400, 200, 404, 204, "LED")
		S.lock(400, 200, 404, 204)
	end,
	check = function(C) return C.lit("LED", 400, 200, 404, 204) and 1 or 0 end,
	solution = function(S)
		S.part(369, 202, "BTRY")
		S.box(370, 202, 396, 202, "METL")
		S.box(397, 202, 399, 202, "PSCN")
	end,
}

add{
	id = "elec-two-lights", cat = "electronics", diff = 1, title = "Two lights",
	text = "Light both LEDs from the one battery.",
	tools = { "METL", "PSCN" }, ink = 500, time = 30,
	stars = { ink = { 300, 430 } },
	hints = { "Wires can branch." },
	setup = function(S)
		S.box(300, 250, 304, 254, "BTRY") S.lock(300, 250, 304, 254)
		S.box(150, 120, 154, 124, "LED") S.lock(150, 120, 154, 124)
		S.box(450, 120, 454, 124, "LED") S.lock(450, 120, 454, 124)
	end,
	check = function(C)
		return ((C.lit("LED", 150, 120, 154, 124) and 0.5 or 0) + (C.lit("LED", 450, 120, 454, 124) and 0.5 or 0))
	end,
	hold = 20,
	solution = function(S)
		S.box(302, 125, 302, 249, "METL")
		S.box(158, 122, 446, 122, "METL")
		S.box(155, 122, 157, 122, "PSCN")
		S.box(447, 122, 449, 122, "PSCN")
		S.box(302, 123, 302, 124, "METL")
	end,
}

add{
	id = "elec-through-wall", cat = "electronics", diff = 2, title = "Through the wall",
	text = "Light the LED behind the insulating wall.",
	tools = { "METL", "PSCN", "WIFI" }, ink = 120, time = 30,
	stars = { ink = { 20, 60 } },
	hints = { "Nothing conducts through insulation.", "WIFI sends a spark to every WIFI on the same channel." },
	setup = function(S)
		S.box(300, 0, 305, 383, "INSL") S.lock(300, 0, 305, 383)
		S.box(100, 198, 104, 202, "BTRY") S.lock(100, 198, 104, 202)
		S.box(500, 198, 504, 202, "LED") S.lock(500, 198, 504, 202)
	end,
	check = function(C) return C.lit("LED", 500, 198, 504, 202) and 1 or 0 end,
	solution = function(S)
		S.box(105, 200, 119, 200, "METL")
		S.box(120, 200, 122, 200, "WIFI")
		S.box(480, 200, 482, 200, "WIFI")
		S.box(483, 200, 499, 200, "PSCN")
	end,
}

add{
	id = "elec-switch", cat = "electronics", diff = 2, title = "Flip the switch",
	text = "The circuit is ready but its switch (SWCH) is off. Turn it on.",
	tools = { "BTRY", "PSCN", "NSCN", "METL" }, ink = 30, time = 30,
	stars = { ink = { 4, 12 } },
	hints = { "A spark through PSCN turns a switch on.", "Touch the switch from above." },
	setup = function(S)
		S.box(200, 200, 200, 200, "BTRY")
		S.box(201, 200, 299, 200, "METL")
		S.box(300, 200, 309, 200, "SWCH")
		S.box(310, 200, 311, 200, "METL") -- a switch does not pass sparks straight on to PSCN
		S.box(312, 200, 314, 200, "PSCN")
		S.box(315, 198, 319, 202, "LED")
		S.lock(190, 185, 299, 215)
		S.lock(300, 200, 309, 215)
		S.lock(310, 185, 330, 215)
	end,
	check = function(C)
		local on = C.count("SWCH", 300, 200, 309, 200, function(i) return life(i) >= 10 end) > 0
		return (on and 0.5 or 0) + (C.lit("LED", 315, 198, 319, 202) and 0.5 or 0)
	end,
	solution = function(S)
		S.part(304, 197, "BTRY")
		S.box(304, 198, 304, 199, "PSCN")
	end,
}

add{
	id = "elec-electrolysis", cat = "electronics", diff = 3, title = "Electrolysis",
	text = "Split water into hydrogen: make 100 particles of H2.",
	tools = { "BTRY", "METL", "PSCN" }, ink = 60, time = 60,
	stars = { time = { 20, 40 }, ink = { 5, 30 } },
	hints = { "Sparked iron splits the water touching it.", "Power the iron electrode." },
	setup = function(S)
		S.floor()
		S.tank(256, 280, 355, 369, "GLAS")
		S.box(305, 285, 305, 360, "IRON") S.lock(305, 285, 305, 360)
		S.box(256, 300, 355, 369, "WATR")
	end,
	check = function(C) return C.count("H2") / 100 end,
	solution = function(S) S.part(305, 284, "BTRY") end,
}

add{
	id = "elec-neon-sign", cat = "electronics", diff = 2, title = "Neon sign",
	text = "Light up the tube of noble gas: make 150 particles of plasma.",
	tools = { "BTRY", "METL", "PSCN" }, ink = 60, time = 30,
	stars = { ink = { 10, 30 } },
	hints = { "Noble gas conducts electricity and glows as plasma.", "Power one of the metal ends of the tube." },
	setup = function(S)
		S.box(198, 198, 401, 199, "GLAS") S.lock(198, 198, 401, 199)
		S.box(198, 210, 401, 211, "GLAS") S.lock(198, 210, 401, 211)
		S.box(198, 200, 199, 209, "METL") S.lock(198, 200, 199, 209)
		S.box(400, 200, 401, 209, "METL") S.lock(400, 200, 401, 209)
		S.box(200, 200, 399, 209, "NBLE")
	end,
	check = function(C) return C.count("PLSM") / 150 end,
	solution = function(S)
		S.part(190, 205, "BTRY")
		S.box(191, 205, 197, 205, "METL")
	end,
}

-- ===== Explosives

add{
	id = "explosives-demolition", cat = "explosives", diff = 1, title = "Demolition",
	text = "Destroy 95% of the brick tower.",
	tools = { "BOMB", "GUNP", "FIRE" }, ink = 120, time = 45,
	stars = { ink = { 60, 90 }, time = { 10, 20 } },
	hints = { "BOMB explodes when it touches something.", "Bombs next to each other go off together; spread them out." },
	setup = function(S, st, C)
		S.floor()
		S.box(280, 222, 330, 371, "BRCK")
		S.lock(280, 222, 330, 371)
		st.bricks = C.count("BRCK")
	end,
	check = function(C, st) return (st.bricks - C.count("BRCK")) / (st.bricks * 0.95) end,
	-- one row of bombs at a time, as a player would drop them
	solution = function(S, st)
		st.steps = {}
		for row = 0, 24 do
			st.steps[#st.steps + 1] = { 1 + row * 30, function(S) for col = 0, 3 do S.part(284 + col * 14, 100, "BOMB") end end }
		end
	end,
}

add{
	id = "explosives-chain", cat = "explosives", diff = 2, title = "Chain reaction",
	text = "Set off all five C-4 charges with one battery.",
	tools = { "FUSE", "METL", "BTRY" }, ink = 800, time = 60,
	stars = { time = { 20, 40 } },
	hints = { "A spark lights a fuse, and a burning fuse sets off C-4 (PLEX).", "Run one fuse past all the charges." },
	setup = function(S, st, C)
		S.floor()
		for k = 0, 4 do S.box(60 + k * 110, 362, 69 + k * 110, 371, "PLEX") S.lock(60 + k * 110, 362, 69 + k * 110, 371) end
		st.plex = C.count("PLEX")
	end,
	check = function(C, st) return (st.plex - C.count("PLEX")) / st.plex end,
	solution = function(S)
		S.box(30, 320, 560, 320, "FUSE")
		for k = 0, 4 do S.box(64 + k * 110, 321, 64 + k * 110, 361, "FUSE") end
		S.part(30, 322, "BTRY")
		S.part(30, 321, "METL")
	end,
}

add{
	id = "explosives-fireworks", cat = "explosives", diff = 2, title = "Fireworks show",
	text = "Fireworks! Make them burst higher than the two towers.",
	tools = { "FIRW", "FIRE" }, ink = 150, time = 30,
	stars = { ink = { 20, 60 }, time = { 8, 15 } },
	hints = { "FIRW flies up when it touches fire." },
	setup = function(S)
		S.floor()
		S.box(40, 260, 49, 371, "BRCK") S.lock(40, 260, 49, 371)
		S.box(562, 260, 571, 371, "BRCK") S.lock(562, 260, 571, 371)
	end,
	check = function(C) return C.count("EMBR", 0, 0, 611, 259, function(i) return sim.partProperty(i, "tmp") == 1 end) / 60 end,
	solution = function(S)
		for k = 0, 4 do
			S.box(150 + k * 70, 366, 153 + k * 70, 371, "FIRW")
			S.part(151 + k * 70, 365, "FIRE")
		end
	end,
}

add{
	id = "explosives-hotter-than-sun", cat = "explosives", diff = 4, title = "Hotter than the sun",
	text = "Reach 6000 C anywhere, without plasma.",
	tools = { "DEUT", "PLUT", "NEUT", "URAN" }, ink = 800, time = 60,
	stars = { ink = { 300, 600 }, time = { 10, 30 } },
	hints = { "Neutrons make plutonium and deuterium react.", "Pack them together." },
	setup = function(S)
		S.floor()
		S.tank(256, 280, 355, 369, "GLAS")
	end,
	-- the hottest moment can be a single frame: track the peak every frame
	each = function(S, st) st.peak = math.max(st.peak or 0, C.maxTemp()) end,
	check = function(C, st) return ((st.peak or 0) - 295.15) / (6273.15 - 295.15) end,
	-- plutonium only reaches MAX_TEMP under pressure, which deuterium makes when neutrons hit it: packed in
	-- deuterium with the neutrons on top it won 500 of 500 seeds; loose in the open tank, 95 of 100
	solution = function(S)
		S.box(256, 366, 355, 369, "DEUT")
		S.box(286, 356, 315, 365, "DEUT")
		S.box(296, 356, 305, 365, "PLUT")
		S.box(296, 350, 305, 355, "NEUT")
	end,
}

-- ===== Chemistry and liquids

add{
	id = "chem-fill-glass", cat = "chemistry", diff = 1, title = "Fill it up",
	text = "Fill the glass to the line. Don't spill!",
	tools = { "WATR" }, ink = 2200, time = 30,
	stars = { ink = { 2050, 2120 } },
	hints = { "Pour slowly from above the glass." },
	setup = function(S)
		S.floor()
		S.tank(281, 300, 330, 369, "GLAS")
		S.box(270, 330, 279, 330, "INVIS") S.lock(270, 330, 279, 330) -- the line
	end,
	check = function(C)
		if C.count("WATR", 0, 0, 278, 383) + C.count("WATR", 333, 0, 611, 383) > 20 then return 0, "spilled" end
		return C.count("WATR", 281, 330, 330, 369) / (50 * 40 * 0.9)
	end,
	hold = 60,
	solution = function(S) S.box(282, 300, 329, 342, "WATR") end,
}

add{
	id = "chem-salt-flats", cat = "chemistry", diff = 2, title = "Salt flats",
	text = "Get 100 particles of salt out of the salt water.",
	tools = { "PLSM", "FIRE", "THRM", "COAL" }, ink = 400, time = 90,
	stars = { time = { 25, 50 }, ink = { 200, 320 } },
	hints = { "Boiling salt water leaves salt behind." },
	setup = function(S)
		S.floor()
		S.tank(256, 300, 355, 339, "TTAN")
		S.box(254, 342, 255, 371, "TTAN") S.lock(254, 342, 255, 371)
		S.box(356, 342, 357, 371, "TTAN") S.lock(356, 342, 357, 371)
		S.box(256, 315, 355, 339, "SLTW")
	end,
	check = function(C) return C.count("SALT") / 100 end,
	solution = function(S) S.box(256, 361, 355, 363, "PLSM") end,
}

add{
	id = "chem-desalination", cat = "chemistry", diff = 3, title = "Desalination",
	text = "Make 150 particles of distilled water (DSTW) from the salt water.",
	tools = { "PLSM", "FIRE", "ICEI", "GLAS", "TTAN" }, ink = 900, time = 120,
	stars = { time = { 40, 80 }, ink = { 500, 750 } },
	hints = { "Steam from salt water is pure.", "Steam turns into distilled water when it touches something cold." },
	setup = function(S)
		S.floor()
		S.tank(256, 300, 355, 339, "TTAN")
		S.box(254, 342, 255, 371, "TTAN") S.lock(254, 342, 255, 371)
		S.box(356, 342, 357, 371, "TTAN") S.lock(356, 342, 357, 371)
		S.box(256, 315, 355, 339, "SLTW")
	end,
	check = function(C) return C.count("DSTW") / 150 end,
	solution = function(S)
		S.box(256, 361, 355, 363, "PLSM")
		S.box(256, 248, 355, 253, "ICEI")
	end,
}

add{
	id = "chem-neutralise", cat = "chemistry", diff = 2, title = "Neutralise",
	text = "Get rid of 90% of the acid.",
	tools = { "BASE", "WATR", "SOAP" }, ink = 1200, time = 90,
	stars = { ink = { 700, 1000 }, time = { 30, 60 } },
	hints = { "Base and acid cancel each other out." },
	setup = function(S, st, C)
		S.floor()
		S.walls(248, 300, 255, 371) S.walls(356, 300, 363, 371)
		S.box(256, 357, 355, 371, "ACID")
		S.lock(256, 300, 355, 371) -- pour in from above; no erasing the acid
		st.acid = C.count("ACID")
	end,
	check = function(C, st) return (st.acid - C.count("ACID")) / (st.acid * 0.9) end,
	solution = function(S) S.box(256, 290, 355, 299, "BASE") end,
}

add{
	id = "chem-rust", cat = "chemistry", diff = 2, title = "Rust bucket",
	text = "Rust 100 particles of iron (it turns into breakable metal, BMTL).",
	tools = { "WATR", "SLTW", "SALT", "O2" }, ink = 2500, time = 60,
	stars = { time = { 15, 30 }, ink = { 1200, 2000 } },
	hints = { "Iron rusts in water.", "Salt water rusts it much faster." },
	setup = function(S)
		S.floor()
		S.tank(256, 300, 355, 369, "GLAS")
		for k = 0, 3 do S.box(270 + k * 22, 320, 271 + k * 22, 369, "IRON") S.lock(270 + k * 22, 320, 271 + k * 22, 369) end
	end,
	check = function(C) return C.count("BMTL") / 100 end,
	solution = function(S) S.box(256, 290, 355, 309, "SLTW") end,
}

add{
	id = "chem-tnt", cat = "chemistry", diff = 2, title = "Make TNT",
	text = "Make 150 particles of TNT (BANG) without setting it off.",
	tools = { "NITR", "OIL", "WATR" }, ink = 300, time = 45,
	stars = { ink = { 120, 220 } },
	hints = { "Nitroglycerin reacts with clay dust (CLST)." },
	setup = function(S)
		S.floor()
		S.tank(256, 330, 355, 369, "GLAS")
		S.box(256, 350, 355, 369, "CLST")
	end,
	check = function(C) return C.count("BANG") / 150 end,
	solution = function(S) S.box(276, 320, 335, 322, "NITR") end,
}

-- ===== Machines and force

add{
	id = "mach-portals", cat = "machines", diff = 2, title = "Portal plumbing",
	text = "Move 300 particles of water into the high tank.",
	tools = { "PRTI", "PRTO" }, ink = 60, time = 60,
	stars = { ink = { 10, 30 }, time = { 15, 30 } },
	hints = { "What goes into a PRTI comes out of a PRTO." },
	setup = function(S)
		S.floor()
		S.tank(100, 300, 199, 369, "GLAS")
		S.box(100, 330, 199, 369, "WATR")
		S.box(420, 140, 421, 371, "GLAS") S.lock(420, 140, 421, 371)
		S.tank(400, 80, 499, 139, "GLAS")
	end,
	check = function(C) return C.count("WATR", 400, 0, 499, 139) / 300 end,
	solution = function(S)
		S.putBox(140, 368, 149, 369, "PRTI")
		S.box(440, 60, 449, 61, "PRTO")
	end,
}

add{
	id = "mach-sorting", cat = "machines", diff = 3, title = "Sorting",
	text = "Separate the mix: 70% of the dust into the bin, and almost no water.",
	tools = { "WALL:LIQD", "WALL:POWDR", "WALL:WALL" }, time = 60,
	stars = { time = { 15, 30 } },
	hints = { "Some walls let only powders through.", "Give the hopper a floor of the right wall before you start." },
	setup = function(S, st, C)
		S.floor()
		S.tank(200, 300, 399, 369, "GLAS") -- the bin
		S.box(218, 120, 219, 203, "GLAS") S.lock(218, 120, 219, 203) -- the hopper has no floor
		S.box(380, 120, 381, 203, "GLAS") S.lock(380, 120, 381, 203)
		for x = 220, 379 do for y = 150, 199 do S.part(x, y, (x + y) % 2 == 0 and "DUST" or "WATR") end end
		st.water, st.dust = C.count("WATR"), C.count("DUST")
	end,
	check = function(C, st)
		if C.count("WATR", 200, 300, 399, 369) > st.water * 0.05 then return 0, "too much water in the bin" end
		return C.count("DUST", 200, 300, 399, 369) / (st.dust * 0.7)
	end,
	hold = 300, -- dust falls faster than water: the bin must stay dry for 5 s
	solution = function(S) S.walls(220, 200, 379, 203, "POWDR") end,
}

add{
	id = "mach-pressure", cat = "machines", diff = 3, title = "Pressure cooker",
	text = "Keep the pressure in the sealed chamber above 50 for 10 seconds.",
	tools = { "PUMP", "BTRY", "PSCN", "METL", "TOOL:HEAT" }, ink = 60, time = 40,
	stars = { ink = { 10, 30 } },
	hints = { "A powered pump sets the pressure to its temperature in C.", "Heat the pump to 100 C, then power it through PSCN." },
	setup = function(S)
		S.floor()
		S.wallBox(260, 300, 351, 359)
	end,
	-- survive-style goal, but it only starts counting once the pressure is reached
	check = function(C, st)
		local _, avg = C.pressureIn(264, 304, 347, 355)
		if avg >= 50 then st.since = st.since or st.frame else st.since = nil end
		return st.since and (st.frame - st.since) / 600 or avg / 50 * 0.2
	end,
	solution = function(S)
		S.box(300, 330, 309, 332, "PUMP", { temp = 373.15 })
		S.box(300, 333, 309, 333, "PSCN")
		S.part(304, 334, "BTRY")
	end,
}

add{
	id = "mach-vacuum", cat = "machines", diff = 3, title = "Vacuum",
	text = "Pull the pressure in the sealed chamber below -50 and keep it there for 5 seconds.",
	tools = { "PUMP", "BTRY", "PSCN", "METL", "TOOL:COOL" }, ink = 60, time = 40,
	stars = { ink = { 10, 30 } },
	hints = { "A pump below 0 C sucks the air out." },
	setup = function(S)
		S.floor()
		S.wallBox(260, 300, 351, 359)
	end,
	check = function(C, st)
		local _, avg = C.pressureIn(264, 304, 347, 355)
		if avg <= -50 then st.since = st.since or st.frame else st.since = nil end
		return st.since and (st.frame - st.since) / 300 or -avg / 50 * 0.2
	end,
	solution = function(S)
		S.box(300, 330, 309, 332, "PUMP", { temp = 173.15 })
		S.box(300, 333, 309, 333, "PSCN")
		S.part(304, 334, "BTRY")
	end,
}

-- ===== Gravity and space

add{
	id = "grav-black-hole", cat = "gravity", diff = 1, title = "Black hole",
	text = "Clean up: get rid of 90% of the dust cloud.",
	tools = { "NBHL" }, ink = 30, time = 60,
	stars = { ink = { 5, 15 }, time = { 15, 30 } },
	hints = { "This challenge turns Newtonian gravity on and normal gravity off.", "A black hole (NBHL) pulls everything in." },
	prepare = spaceOn, cleanup = spaceOff,
	setup = function(S, st, C)
		for x = 200, 400, 3 do for y = 100, 250, 3 do S.part(x, y, "DUST", { vx = 0, vy = 0 }) end end
		S.lock(195, 95, 405, 255) -- no erasing the cloud
		st.dust = C.count("DUST")
	end,
	check = function(C, st) return (st.dust - C.count("DUST")) / (st.dust * 0.9) end,
	solution = function(S) S.box(298, 280, 302, 284, "NBHL") end,
}

-- ===== Life and nature

add{
	id = "life-green-thumb", cat = "life", diff = 1, title = "Green thumb",
	text = "Grow 400 particles of plant.",
	tools = { "PLNT", "WATR" }, ink = 40, time = 60,
	stars = { ink = { 5, 20 }, time = { 20, 40 } },
	hints = { "Plants drink water and grow." },
	setup = function(S)
		S.floor()
		S.tank(256, 330, 355, 369, "GLAS")
		S.box(256, 340, 355, 369, "WATR")
	end,
	check = function(C) return C.count("PLNT") / 400 end,
	solution = function(S) S.box(300, 338, 309, 339, "PLNT") end,
}

add{
	id = "life-glider", cat = "life", diff = 2, title = "Glider",
	text = "Build a Game of Life glider that flies into the box on the right.",
	tools = { "LIFE:GOL" }, ink = 12, time = 60,
	stars = { ink = { 5, 8 } },
	hints = { "A glider is five cells: .X. / ..X / XXX", "It moves one cell diagonally every four generations." },
	setup = function(S)
		S.lock(405, 0, 611, 383) -- the box: nothing can be drawn there
	end,
	check = function(C)
		local n = C.count("LIFE", 405, 0, 611, 383)
		return (n >= 3 and n <= 12) and 1 or 0
	end,
	solution = function(S)
		for _, c in ipairs({ { 1, 0 }, { 2, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 } }) do
			S.part(330 + c[1], 160 + c[2], "LIFE", { ctype = 0 })
		end
	end,
}

add{
	id = "life-outbreak", cat = "life", diff = 2, title = "Outbreak",
	text = "Stop the virus: keep it out of the town for 30 seconds.",
	tools = { "WALL:WALL", "SOAP", "WATR" }, survive = 30,
	failText = "the virus reached the town",
	hints = { "The virus spreads by touch.", "Cut the bridge (the eraser always works)." },
	setup = function(S)
		S.floor()
		S.box(20, 362, 591, 371, "WOOD")
		S.lock(20, 362, 39, 371)
		S.lock(480, 340, 591, 371)
		S.box(480, 340, 591, 361, "WOOD")
		S.box(20, 352, 39, 361, "VIRS")
	end,
	check = function(C)
		return C.count("VIRS", 470, 0, 611, 383) + C.count("VRSS", 470, 0, 611, 383) + C.count("VRSG", 470, 0, 611, 383) == 0 and 1 or 0
	end,
	solution = function(S)
		for x = 200, 209 do for y = 362, 371 do
			local id = sim.partID(x, y)
			if id then sim.partKill(id) end
		end end
	end,
}

-- ===== Characters

add{
	id = "char-fighters", cat = "characters", diff = 2, title = "Pest control",
	text = "Get rid of the three fighters in the pit.",
	tools = { "LAVA", "ACID", "FIRE", "WATR" }, ink = 700, time = 45,
	stars = { ink = { 250, 450 }, time = { 10, 25 } },
	hints = { "Fighters don't like heat." },
	setup = function(S)
		S.floor()
		S.tank(200, 250, 411, 369, "GLAS")
		for k = 0, 2 do S.part(250 + k * 50, 320, "FIGH") end
	end,
	check = function(C) return C.count("FIGH") == 0 and 1 or 0 end,
	solution = function(S) S.box(205, 300, 406, 302, "LAVA") end,
}

-- ===== Puzzles and records

add{
	id = "puzzle-minimalist", cat = "puzzles", diff = 3, title = "Minimalist",
	text = "Light the far LED using as few particles as you can.",
	tools = { "METL", "PSCN", "WIFI" }, ink = 400, time = 30,
	stars = { ink = { 8, 60 } },
	hints = { "Wires are expensive. Is there a wireless way?" },
	setup = function(S)
		S.box(40, 40, 44, 44, "BTRY") S.lock(40, 40, 44, 44)
		S.box(560, 330, 564, 334, "LED") S.lock(560, 330, 564, 334)
	end,
	check = function(C) return C.lit("LED", 560, 330, 564, 334) and 1 or 0 end,
	solution = function(S)
		S.part(45, 42, "METL")
		S.part(46, 42, "WIFI")
		S.part(558, 332, "WIFI")
		S.part(559, 332, "PSCN")
	end,
}

add{
	id = "puzzle-speedrun", cat = "puzzles", diff = 2, title = "Speed melt",
	text = "Melt 90% of the iron block, as fast as you can.",
	tools = { "PLSM", "THRM", "FIRE", "LAVA" }, ink = 1200, time = 60,
	stars = { time = { 5, 15 } },
	hints = { "Thermite burns hotter than anything else here." },
	setup = function(S, st, C)
		S.floor()
		S.box(286, 342, 325, 371, "IRON") S.lock(286, 342, 325, 371)
		st.iron = C.count("IRON")
	end,
	check = function(C, st) return (st.iron - C.count("IRON")) / (st.iron * 0.9) end,
	solution = function(S)
		S.box(286, 330, 325, 341, "THRM")
		S.part(300, 329, "FIRE")
	end,
}

add{
	id = "elec-heat-switch", cat = "electronics", diff = 2, title = "Heat switch",
	text = "The circuit has a gap of NTCT, which only conducts when hot. Light the LED.",
	tools = { "FIRE", "PLSM", "LAVA", "COAL" }, ink = 100, time = 30,
	stars = { ink = { 20, 60 }, time = { 5, 15 } },
	hints = { "NTCT conducts above 100 C.", "Plasma and lava are so hot that they melt the wire. Use something gentler." },
	setup = function(S)
		S.box(200, 200, 200, 200, "BTRY")
		S.box(201, 200, 286, 200, "METL")
		S.box(287, 200, 289, 200, "PSCN") -- NTCT takes sparks from PSCN only when it is hot
		S.box(290, 200, 299, 200, "NTCT")
		S.box(300, 200, 302, 200, "PSCN")
		S.box(303, 200, 389, 200, "METL")
		S.box(390, 200, 392, 200, "PSCN")
		S.box(393, 198, 397, 202, "LED")
		S.lock(195, 195, 289, 205) S.lock(300, 195, 400, 205) S.lock(290, 200, 299, 200)
	end,
	check = function(C) return C.lit("LED", 393, 198, 397, 202) and 1 or 0 end,
	hold = 20,
	solution = function(S) S.box(290, 201, 299, 203, "FIRE") end,
}

add{
	id = "explosives-surgical", cat = "explosives", diff = 3, title = "Surgical strike",
	text = "Knock down 90% of the tower, but keep 95% of the house next to it.",
	tools = { "BOMB", "PLEX", "FIRE" }, ink = 80, time = 45,
	stars = { ink = { 40, 60 } },
	hints = { "Bombs are precise: each one only clears a small circle.", "Drop them one at a time, right on top of the tower." },
	setup = function(S, st, C)
		S.floor()
		S.box(250, 252, 269, 371, "BRCK") S.lock(250, 252, 269, 371)
		S.box(430, 332, 479, 371, "BRCK") S.lock(430, 332, 479, 371)
		st.tower = C.count("BRCK", 250, 0, 269, 383)
		st.house = C.count("BRCK", 430, 0, 479, 383)
	end,
	check = function(C, st)
		if C.count("BRCK", 430, 0, 479, 383) < st.house * 0.95 then return 0, "the house was damaged" end
		return (st.tower - C.count("BRCK", 250, 0, 269, 383)) / (st.tower * 0.9)
	end,
	solution = function(S, st)
		st.steps = {}
		for k = 0, 31 do st.steps[#st.steps + 1] = { 1 + k * 20, function(S) S.part(k % 2 == 0 and 255 or 264, 220, "BOMB") end } end
	end,
}

add{
	id = "explosives-crater", cat = "explosives", diff = 2, title = "Crater",
	text = "Blast a hole through the brick shelf in the marked spot.",
	tools = { "BOMB", "PLEX", "GUNP", "FIRE" }, ink = 60, time = 30,
	stars = { ink = { 3, 20 } },
	hints = { "Each bomb clears a circle 16 pixels across." },
	setup = function(S)
		S.box(0, 300, 611, 319, "BRCK") -- solid (stone would be a powder)
		S.lock(0, 300, 611, 319)
		S.box(296, 296, 296, 299, "INVIS") S.box(315, 296, 315, 299, "INVIS") -- markers
		S.lock(296, 296, 296, 299) S.lock(315, 296, 315, 299)
	end,
	check = function(C) return (1 - C.count("BRCK", 300, 300, 311, 319) / (12 * 20)) / 0.9 end,
	solution = function(S, st)
		st.steps = {}
		for k = 0, 5 do st.steps[#st.steps + 1] = { 1 + k * 40, function(S) S.part(305, 250, "BOMB") end } end
	end,
}

add{
	id = "chem-oil-spill", cat = "chemistry", diff = 2, title = "Oil spill",
	text = "Get rid of 90% of the oil on the lake, and keep 80% of the water.",
	tools = { "FIRE", "PLSM", "SOAP", "WATR" }, ink = 50, time = 60,
	stars = { ink = { 3, 20 }, time = { 15, 30 } },
	hints = { "Oil burns. Water doesn't." },
	setup = function(S, st, C)
		S.floor()
		S.walls(148, 280, 155, 371) S.walls(456, 280, 463, 371)
		S.box(156, 320, 455, 371, "WATR")
		S.box(156, 310, 455, 319, "OIL")
		S.lock(156, 310, 455, 371)
		st.oil, st.water = C.count("OIL"), C.count("WATR")
	end,
	check = function(C, st)
		if C.count("WATR") < st.water * 0.8 then return 0, "too much water boiled away" end
		return (st.oil - C.count("OIL")) / (st.oil * 0.9)
	end,
	solution = function(S) S.part(300, 309, "FIRE") S.part(200, 309, "FIRE") S.part(400, 309, "FIRE") end,
}

add{
	id = "chem-lava-stone", cat = "chemistry", diff = 2, title = "Lava to stone",
	text = "Cool the lava pool until 90% of it has set into stone.",
	tools = { "WATR", "ICEI", "SNOW", "LNTG" }, ink = 4000, time = 60,
	stars = { ink = { 2000, 3200 }, time = { 25, 40 } },
	hints = { "Lava sets into stone below about 700 C.", "Lots of water, all at once." },
	setup = function(S, st, C)
		S.floor()
		S.walls(248, 300, 255, 371) S.walls(356, 300, 363, 371)
		S.box(256, 367, 355, 371, "LAVA")
		S.lock(256, 350, 355, 371)
		st.lava = C.count("LAVA")
	end,
	check = function(C, st) return C.count("STNE") / (st.lava * 0.9) end,
	solution = function(S) S.box(256, 230, 355, 267, "WATR") end,
}

add{
	id = "chem-salt-water", cat = "chemistry", diff = 1, title = "Seawater",
	text = "Turn 1000 particles of the fresh water into salt water.",
	tools = { "SALT" }, ink = 600, time = 45,
	stars = { ink = { 250, 450 }, time = { 20, 35 } },
	hints = { "Salt dissolves in water." },
	setup = function(S)
		S.floor()
		S.tank(256, 300, 355, 369, "GLAS")
		S.box(256, 330, 355, 369, "WATR")
		S.lock(256, 320, 355, 369)
	end,
	check = function(C) return C.count("SLTW") / 1000 end,
	solution = function(S) S.box(256, 300, 355, 304, "SALT") end,
}

add{
	id = "mach-clone", cat = "machines", diff = 1, title = "Copy machine",
	text = "Make 1000 particles of water. You have no water tool, only a few drops.",
	tools = { "CLNE" }, ink = 20, time = 45,
	stars = { ink = { 2, 8 }, time = { 10, 20 } },
	hints = { "CLNE copies whatever touches it first." },
	setup = function(S)
		S.floor()
		S.tank(206, 200, 405, 369, "GLAS")
		S.box(300, 365, 309, 369, "WATR")
	end,
	check = function(C) return C.count("WATR") / 1000 end,
	solution = function(S) S.box(300, 364, 301, 364, "CLNE") end,
}

add{
	id = "life-tree", cat = "life", diff = 2, title = "Plant a tree",
	text = "Grow 150 particles of tree (wood and leaves) from a seed.",
	tools = { "SEED", "WATR" }, ink = 600, time = 90,
	stars = { ink = { 150, 400 }, time = { 30, 60 } },
	hints = { "Seeds need soil (DUST, SAND or CLST), water and warmth." },
	setup = function(S)
		S.floor()
		S.walls(200, 340, 207, 371) S.walls(404, 340, 411, 371)
		S.box(208, 360, 403, 371, "DUST")
		S.lock(208, 360, 403, 371)
	end,
	check = function(C) return (C.count("WOOD") + C.count("PLNT")) / 150 end,
	solution = function(S)
		S.box(300, 357, 302, 359, "SEED")
		S.box(250, 330, 350, 334, "WATR")
	end,
}

add{
	id = "life-aquarium", cat = "life", diff = 2, title = "Aquarium",
	text = "The fish tank is dry. Get water to the fish within 5 seconds and keep at least 3 of them alive in it for 30 seconds.",
	tools = { "WATR", "DSTW" }, ink = 3000, survive = 30, failText = "the fish died",
	hints = { "Fish need water. Quickly!" },
	setup = function(S)
		S.floor()
		S.tank(256, 280, 355, 369, "GLAS")
		for k = 0, 4 do S.part(266 + k * 20, 365, "FISH") end
	end,
	-- a fish out of water only dies by chance (1 in 600 a frame), so a dry tank kept 3 of 5 alive for 30 seconds
	-- about once in 900 tries: after 5 seconds only fish touching water (as FISH.cpp sees it) count
	check = function(C, st)
		if C.count("FISH") < 3 then return 0 end
		if st.frame < 5 * 60 then return 1 end
		local water = { [elem.DEFAULT_PT_WATR] = true, [elem.DEFAULT_PT_DSTW] = true, [elem.DEFAULT_PT_SLTW] = true, [elem.DEFAULT_PT_CBNW] = true }
		local wet = C.count("FISH", nil, nil, nil, nil, function(i)
			local x, y = sim.partPosition(i)
			x, y = math.floor(x + 0.5), math.floor(y + 0.5)
			for dy = -1, 1 do
				for dx = -1, 1 do
					local r = sim.pmap(x + dx, y + dy)
					if r and water[sim.partProperty(r, "type")] then return true end
				end
			end
			return false
		end)
		if wet < 3 then return 0, "the fish are out of water" end
		return 1
	end,
	solution = function(S) S.box(256, 345, 355, 369, "WATR") end,
}

add{
	id = "char-shelter", cat = "characters", diff = 2, title = "Shelter",
	text = "Lava will rain down soon. Keep the stickman safe for 30 seconds (badly burned counts as dead).",
	tools = { "WALL:WALL", "GLAS", "STNE", "INSL" }, ink = 400, survive = 30, failText = "the stickman died",
	hints = { "Build a roof.", "Walls don't get hot." },
	setup = function(S)
		S.floor()
		S.walls(244, 300, 247, 371) S.walls(364, 300, 367, 371) -- walls: glass would pass the lava's heat on
		S.part(305, 350, "STKM")
	end,
	each = function(S, st)
		if st.frame >= 180 and st.frame < 900 and st.frame % 2 == 0 then
			for k = 1, 3 do S.part(math.random(240, 371), 5, "LAVA") end
			-- and a steady drip right above the stickman, so an unsheltered one is always hit
			if st.frame % 10 == 0 then S.part(300 + (st.frame / 10) % 11, 5, "LAVA") end
		end
	end,
	-- the stickman heals, floats up through lava and can respawn between checks, so "is it there" misses
	-- deaths: a badly burned one (life under 50) counts as dead
	check = function(C)
		local alive = false
		for i in sim.parts() do
			if sim.partProperty(i, "type") == elem.DEFAULT_PT_STKM then
				if sim.partProperty(i, "life") < 50 then return 0, "the stickman got badly burned" end
				alive = true
			end
		end
		return alive and 1 or 0
	end,
	solution = function(S)
		S.walls(232, 280, 379, 283, "WALL")
	end,
}

add{
	id = "puzzle-ramp", cat = "puzzles", diff = 2, title = "Ramp",
	text = "Guide the falling sand into the bin: 150 grains.",
	tools = { "GLAS", "STNE", "WALL:WALL" }, ink = 700, time = 60,
	stars = { ink = { 300, 550 } },
	hints = { "Powders slide down slopes." },
	setup = function(S)
		S.floor()
		S.tank(450, 320, 549, 369, "GLAS")
	end,
	each = function(S, st)
		if st.frame < 2400 and st.frame % 2 == 0 then S.part(300 + st.frame % 6, 20, "SAND") end
	end,
	check = function(C) return C.count("SAND", 450, 320, 549, 369) / 150 end,
	solution = function(S)
		for k = 0, 179 do S.part(290 + k, 60 + k, "GLAS") S.part(291 + k, 60 + k, "GLAS") end
	end,
}


add{
	id = "elec-christmas", cat = "electronics", diff = 2, title = "Christmas lights",
	text = "Light all five LEDs at the same time.",
	tools = { "BTRY", "METL", "PSCN" }, ink = 700, time = 40,
	stars = { ink = { 400, 600 } },
	hints = { "One long wire can feed many lights." },
	setup = function(S)
		for k = 0, 4 do S.box(100 + k * 100, 150, 104 + k * 100, 154, "LED") S.lock(100 + k * 100, 150, 104 + k * 100, 154) end
	end,
	check = function(C)
		local n = 0
		for k = 0, 4 do if C.lit("LED", 100 + k * 100, 150, 104 + k * 100, 154) then n = n + 1 end end
		return n / 5
	end,
	hold = 20,
	solution = function(S)
		S.part(80, 180, "BTRY")
		S.box(81, 180, 520, 180, "METL")
		for k = 0, 4 do S.box(102 + k * 100, 155, 102 + k * 100, 157, "PSCN") S.box(102 + k * 100, 158, 102 + k * 100, 179, "METL") end
	end,
}

add{
	id = "grav-white-hole", cat = "gravity", diff = 2, title = "White hole",
	text = "Push 90% of the dust off the screen.",
	tools = { "NWHL" }, ink = 30, time = 60,
	stars = { ink = { 5, 15 }, time = { 15, 30 } },
	hints = { "This challenge turns Newtonian gravity on and normal gravity off.", "A white hole (NWHL) pushes everything away." },
	prepare = spaceOn, cleanup = spaceOff,
	setup = function(S, st, C)
		for x = 250, 360, 3 do for y = 140, 240, 3 do S.part(x, y, "DUST", { vx = 0, vy = 0 }) end end
		S.lock(245, 135, 365, 245)
		st.dust = C.count("DUST")
	end,
	check = function(C, st) return (st.dust - C.count("DUST")) / (st.dust * 0.9) end,
	-- beside the cloud, so the dust leaves sideways as one group: 100 of 100 runs won within 780 frames.
	-- Below it (303, 250) the dust spread out and drifted with the air it stirred up: about 1 run in 60
	-- ended at 98-99%.
	solution = function(S) S.box(368, 188, 372, 192, "NWHL") end,
}

add{
	id = "life-wildfire", cat = "life", diff = 2, title = "Wildfire",
	text = "A fire will start in the forest. Save 60% of the trees.",
	tools = { "WATR", "WALL:WALL", "SAND" }, ink = 2000, survive = 30, failText = "the forest burned",
	hints = { "Fire needs something to burn to keep going.", "Make a gap (the eraser works outside the burning edge)." },
	setup = function(S, st, C)
		S.floor()
		S.box(20, 330, 591, 371, "PLNT")
		S.lock(20, 330, 60, 371)
		st.plants = C.count("PLNT")
	end,
	-- the fire is lit for two seconds, so it always catches
	each = function(S, st)
		if st.frame >= 60 and st.frame <= 180 and st.frame % 10 == 0 then S.box(20, 325, 40, 329, "FIRE") end
	end,
	check = function(C, st) return C.count("PLNT") >= st.plants * 0.6 and 1 or 0 end,
	solution = function(S)
		for x = 110, 169 do for y = 330, 371 do
			local id = sim.partID(x, y)
			if id then sim.partKill(id) end
		end end
	end,
}

add{
	id = "chem-make-water", cat = "chemistry", diff = 2, title = "Rocket fuel",
	text = "Burn the hydrogen to make 150 particles of water (steam counts).",
	tools = { "FIRE", "PLSM" }, ink = 20, time = 40,
	stars = { ink = { 1, 5 }, time = { 5, 15 } },
	hints = { "Hydrogen burns with oxygen and leaves water behind." },
	setup = function(S)
		S.floor()
		S.wallBox(256, 280, 355, 367)
		for x = 256, 355 do for y = 300, 367 do S.part(x, y, (x + y) % 3 == 0 and "O2" or "H2") end end
	end,
	check = function(C) return (C.count("WATR") + C.count("DSTW") + C.count("WTRV")) / 150 end,
	solution = function(S) S.put(300, 310, "FIRE") end,
}

add{
	id = "heat-ice-wall", cat = "heat", diff = 1, title = "Ice dam",
	text = "Melt through the ice dam so 500 particles of water reach the field on the right.",
	tools = { "FIRE", "PLSM", "LAVA", "SALT" }, ink = 300, time = 60,
	stars = { ink = { 100, 200 }, time = { 20, 35 } },
	hints = { "Plasma melts ice fast.", "Put the heat on top of the dam; it melts its way down." },
	setup = function(S)
		S.floor()
		S.box(100, 290, 249, 371, "WATR", { temp = 275.15 }) -- ice-cold, or it would melt the dam by itself
		S.box(250, 250, 269, 371, "ICEI", { temp = 243.15 })
		S.box(98, 250, 99, 371, "GLAS") S.lock(98, 250, 99, 371)
		S.lock(250, 250, 269, 371) S.lock(100, 290, 249, 371)
	end,
	check = function(C) return (C.count("WATR", 280, 0, 611, 383) + C.count("DSTW", 280, 0, 611, 383)) / 500 end,
	solution = function(S) S.box(250, 240, 269, 249, "PLSM") end,
}

add{
	id = "puzzle-tower", cat = "puzzles", diff = 1, title = "Reach for the sky",
	text = "Build a brick tower from the floor up to the flag, with no gaps.",
	tools = { "BRCK" }, ink = 300, time = 30,
	stars = { ink = { 275, 290 } },
	hints = { "A straight line is the shortest tower." },
	setup = function(S)
		S.floor()
		S.box(400, 95, 415, 99, "GOLD") S.lock(400, 95, 415, 99) -- the flag
	end,
	check = function(C)
		local rows = {}
		for i in sim.parts() do
			if sim.partProperty(i, "type") == elem.DEFAULT_PT_BRCK then
				local x, y = sim.partPosition(i)
				rows[math.floor(y + 0.5)] = true
			end
		end
		-- every row from the floor up must have brick in it; count how high that goes
		local m = 0
		for y = 371, 100, -1 do if rows[y] then m = m + 1 else break end end
		return m / 272
	end,
	solution = function(S) S.box(300, 100, 300, 371, "BRCK") end,
}

add{
	id = "chem-acid-etch", cat = "chemistry", diff = 1, title = "Etching",
	text = "Dissolve 75% of the metal block with acid.",
	tools = { "ACID" }, ink = 2000, time = 60,
	stars = { ink = { 800, 1400 }, time = { 15, 30 } },
	hints = { "Acid eats almost anything except glass." },
	setup = function(S, st, C)
		S.floor()
		S.tank(256, 300, 355, 369, "GLAS")
		S.box(286, 340, 325, 369, "METL")
		S.lock(286, 340, 325, 369)
		st.metal = C.count("METL")
	end,
	check = function(C, st) return (st.metal - C.count("METL")) / (st.metal * 0.75) end,
	solution = function(S) S.box(256, 282, 355, 299, "ACID") end,
}

-- ---------------------------------------------------------------- order
-- by category, easiest first (the order of the lists and of "Next challenge")
do
	local catIndex = {}
	for i, c in ipairs(CATEGORIES) do catIndex[c.id] = i end
	for i, d in ipairs(list) do d.order = i end
	table.sort(list, function(a, b)
		if catIndex[a.cat] ~= catIndex[b.cat] then return catIndex[a.cat] < catIndex[b.cat] end
		if a.diff ~= b.diff then return a.diff < b.diff end
		return a.order < b.order
	end)
end
