// Serves this folder on http://127.0.0.1:8123/ and opens it in the default browser.
// Needs only Node.js 18+ (no packages, no administrator rights). Stop with Ctrl+C.
// Set NO_BROWSER=1 to skip opening the browser.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.dirname(fileURLToPath(import.meta.url));
const PORTS = [8123, 8124, 8125]; // saves are stored per address, so keep the port stable
const MIME = {
	'.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
	'.json': 'application/json', '.wasm': 'application/wasm', '.webmanifest': 'application/manifest+json', '.png': 'image/png', '.txt': 'text/plain; charset=utf-8',
	'.gz': 'application/gzip', '.patch': 'text/plain; charset=utf-8',
};

function handler(req, res) {
	let rel = decodeURIComponent(new URL(req.url, 'http://localhost').pathname);
	if (rel.endsWith('/')) rel += 'index.html';
	const file = path.join(ROOT, rel);
	if (!file.startsWith(ROOT) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
		res.writeHead(404, { 'Content-Type': 'text/plain' });
		return res.end('not found');
	}
	res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream', 'Cache-Control': 'no-cache' });
	fs.createReadStream(file).pipe(res);
}

function listen(i) {
	const server = http.createServer(handler);
	server.once('error', () => {
		if (i + 1 < PORTS.length) listen(i + 1);
		else { console.error(`Ports ${PORTS.join(', ')} are all in use; close the other copy of the game first.`); process.exit(1); }
	});
	server.listen(PORTS[i], '127.0.0.1', () => {
		const url = `http://127.0.0.1:${PORTS[i]}/`;
		console.log(`Cracker1000's Mod is running at ${url}  (press Ctrl+C to stop)`);
		if (!process.env.NO_BROWSER) {
			const [cmd, args] = process.platform === 'win32' ? ['cmd', ['/c', 'start', '', url]]
				: process.platform === 'darwin' ? ['open', [url]] : ['xdg-open', [url]];
			spawn(cmd, args, { stdio: 'ignore', detached: true }).on('error', () => {}).unref();
		}
	});
}
listen(0);
