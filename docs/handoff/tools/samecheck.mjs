// Checks that two builds simulate exactly the same: for each scene, seeds the random number generators
// (tpt_rng_seed), builds the scene, steps it with tpt_benchmark and writes every particle's properties and the
// air grids to a file, at several frame counts. Prints a digest per scene and frame count for each build and
// whether they match. Use the single-threaded build (?threads=0 is added): gravity runs on its own thread in
// the threaded one.
//   node samecheck.mjs http://127.0.0.1:8831/ http://127.0.0.1:8832/
// (serve each dist/ copy with tests/browser/serve.mjs <dir> <port>)
import { createHash } from 'node:crypto';
import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';

const urls = process.argv.slice(2);
const STEPS = (process.env.STEPS || '1,30,120,400').split(',').map(Number);
const SEED = Number(process.env.SEED || 12345);

// Rows of conductors, each fed by a battery, with the elements SPRK treats specially as receivers and senders.
const ROW_TYPES = process.env.ROWS ? process.env.ROWS.split(',') : ['METL', 'PSCN', 'NSCN', 'INST', 'SWCH', 'NTCT', 'PTCT', 'WATR', 'SLTW', 'INWR', 'ETRD', 'IRON', 'TUNG',
	'QRTZ', 'RSST', 'COPR', 'BMTL', 'GOLD', 'NICH', 'LCRY', 'PUMP', 'LEAD', 'IRDM', 'TESC', 'NBLE', 'EMP', 'PPIP', 'BRMT'];
const SCENES = {
	electronics: `for row = 0, 19 do local y = 20 + row * 18 for x = 20, 590 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end
		for x = 20, 590 do sim.partCreate(-1, x, y + 1, (x % 2 == 0) and elem.DEFAULT_PT_PSCN or elem.DEFAULT_PT_NSCN) end
		for x = 20, 590, 40 do sim.partCreate(-1, x, y - 1, elem.DEFAULT_PT_BTRY) end
		for x = 20, 590 do sim.partCreate(-1, x, y + 2, elem.DEFAULT_PT_COPR or elem.DEFAULT_PT_METL) end end`,
	circuits: `local types = { ${ROW_TYPES.map((t) => `"${t}"`).join(', ')} }
		for row, name in ipairs(types) do
			local t = elem["DEFAULT_PT_" .. name]
			local y = 6 + row * 13
			if t then
				sim.partCreate(-1, 10, y, elem.DEFAULT_PT_BTRY)
				for x = 11, 200 do sim.partCreate(-1, x, y, t) end
				for x = 201, 260 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end
				for x = 261, 300 do sim.partCreate(-1, x, y, (x % 3 == 0) and elem.DEFAULT_PT_PSCN or elem.DEFAULT_PT_NSCN) end
				for x = 301, 400 do sim.partCreate(-1, x, y, t) end
				sim.partCreate(-1, 300, y + 1, elem.DEFAULT_PT_INSL)
				for x = 401, 590 do sim.partCreate(-1, x, y, (math.floor(x / 10) % 2 == 0) and t or elem.DEFAULT_PT_METL) end
				for x = 30, 590, 37 do sim.partCreate(-1, x, y + 1, t) sim.partCreate(-1, x + 1, y + 2, elem.DEFAULT_PT_METL) end
			end
		end`,
	mixed: `for x = 20, 590 do for y = 300, 379 do sim.partCreate(-1, x, y, (x % 7 == 0) and elem.DEFAULT_PT_SAND or elem.DEFAULT_PT_WATR) end end
		for x = 20, 590 do for y = 200, 230 do sim.partCreate(-1, x, y, (x % 5 == 0) and elem.DEFAULT_PT_WOOD or elem.DEFAULT_PT_DUST) end end
		for x = 20, 590, 4 do sim.partCreate(-1, x, 195, elem.DEFAULT_PT_FIRE) end
		for x = 100, 140 do for y = 100, 140 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLSM) end end
		sim.createWallBox(0, 0, 611, 3, 8) sim.createWallBox(0, 380, 611, 383, 8) sim.createWallBox(0, 0, 3, 383, 8) sim.createWallBox(608, 0, 611, 383, 8)`,
	// two time dilation fields (TIME with tmp2 set): faster on the left, slower on the right, with plasma
	// stirring the air in both, so the air code's time dilation branches run
	timefield: `for k, spec in ipairs({ { 150, 273.15 + 16 * 3 }, { 450, 273.15 - 16 * 3 } }) do
			local cx, temp = spec[1], spec[2]
			local id = sim.partCreate(-1, cx, 190, elem.DEFAULT_PT_TIME)
			sim.partProperty(id, "temp", temp) sim.partProperty(id, "tmp2", 1000000)
			for x = cx - 40, cx + 40 do for y = 120, 140 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLSM) end end
			for x = cx - 40, cx + 40 do for y = 240, 260 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_GAS) end end
			for x = cx - 40, cx + 40, 5 do sim.partCreate(-1, x, 239, elem.DEFAULT_PT_FIRE) end
		end`,
};

// One line per particle (every property the game exposes, floats with 9 significant digits, enough for an
// exact float32) and the air grids.
const DUMP = (path) => `
	local props = { "type", "ctype", "life", "tmp", "tmp2", "x", "y", "vx", "vy", "temp", "flags", "dcolour" }
	for _, extra in ipairs({ "tmp3", "tmp4", "pavg0", "pavg1" }) do
		if pcall(sim.partProperty, 0, extra) then props[#props + 1] = extra end
	end
	local out = {}
	for i in sim.parts() do
		local row = { i }
		for _, p in ipairs(props) do
			local ok, v = pcall(sim.partProperty, i, p)
			row[#row + 1] = ok and (math.floor(v) == v and string.format("%d", v) or string.format("%.9g", v)) or "-"
		end
		out[#out + 1] = table.concat(row, " ")
	end
	for y = 0, 95 do
		local row = { "air", y }
		for x = 0, 152 do
			row[#row + 1] = string.format("%.9g %.9g %.9g", sim.pressure(x, y), sim.velocityX(x, y), sim.velocityY(x, y))
		end
		out[#out + 1] = table.concat(row, " ")
	end
	local f = io.open("${path}", "w") f:write(table.concat(out, "\\n")) f:close()`;

async function run(url) {
	const browser = await chromium.launch();
	const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
	await page.goto(url + (url.includes('?') ? '&' : '?') + 'threads=0');
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
	await page.waitForTimeout(1000);
	let n = 0;
	const lua = async (code) => {
		const out = `/tmp/samecheck${++n}.done`;
		await page.evaluate(([c]) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [c]),
			[`${code.replace(/\s*\n\s*/g, ' ')} local f = io.open("${out}", "w") f:write("1") f:close()`]);
		for (let k = 0; k < 2000; k++) {
			const done = await page.evaluate((p) => { try { window.tptPage.module.FS.readFile(p); return true; } catch (e) { return false; } }, out);
			if (done) return;
			await page.waitForTimeout(10);
		}
		throw new Error('Lua command did not finish: ' + code.slice(0, 80));
	};
	const digests = {};
	await lua('tpt.set_pause(1)');
	for (const [name, scene] of Object.entries(SCENES).filter(([n]) => !process.env.SCENES || process.env.SCENES.split(',').includes(n))) {
		await lua('sim.clearSim()');
		await page.evaluate((s) => window.tptPage.module.ccall('tpt_rng_seed', null, ['number'], [s]), SEED);
		await lua(scene);
		let done = 0;
		for (const steps of STEPS) {
			// re-seeded in the same call as the stepping: the game draws frames while waiting for Lua, and those
			// advance the sand colour cycle
			await page.evaluate(([f, s]) => {
				const m = window.tptPage.module;
				m.ccall('tpt_rng_seed', null, ['number'], [s]);
				m.ccall('tpt_benchmark', 'number', ['number', 'number'], [f, 0]);
			}, [steps - done, SEED + steps]);
			done = steps;
			const path = `/tmp/samecheck-${name}-${steps}.txt`;
			await lua(DUMP(path));
			const text = await page.evaluate((p) => new TextDecoder().decode(window.tptPage.module.FS.readFile(p)), path);
			const particles = text.split('\n').filter((l) => !l.startsWith('air')).length;
			digests[`${name}@${steps}`] = { hash: createHash('sha256').update(text).digest('hex').slice(0, 16), particles };
		}
	}
	await browser.close();
	return digests;
}

const results = [];
for (const url of urls) results.push(await run(url));
let same = true;
for (const key of Object.keys(results[0])) {
	const row = results.map((r) => `${r[key].hash} (${r[key].particles} particles)`);
	const match = results.every((r) => r[key].hash === results[0][key].hash);
	same = same && match;
	console.log(`${match ? 'same' : 'DIFF'} ${key.padEnd(18)} ${row.join('  ')}`);
}
console.log(same ? 'ALL SAME' : 'DIFFERENT');
process.exit(same ? 0 : 1);
