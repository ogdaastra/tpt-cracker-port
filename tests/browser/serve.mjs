// Minimal static server for dist/ used by the browser tests (never file://). Applies the same
// response headers as dist/vercel.json so local runs see what the deployment serves.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const MIME = {
	'.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
	'.json': 'application/json; charset=utf-8', '.wasm': 'application/wasm', '.webmanifest': 'application/manifest+json', '.txt': 'text/plain; charset=utf-8',
	'.gz': 'application/gzip', '.patch': 'text/plain; charset=utf-8', '.png': 'image/png',
};

export function startServer(root, port = 0) {
	root = path.resolve(root);
	let rules = [];
	try {
		const cfg = JSON.parse(fs.readFileSync(path.join(root, 'vercel.json'), 'utf8'));
		rules = (cfg.headers || []).map((r) => ({ re: new RegExp('^' + r.source + '$'), headers: r.headers }));
	} catch (e) { /* no vercel.json: plain static serving */ }
	const server = http.createServer((req, res) => {
		const url = new URL(req.url, 'http://localhost');
		let rel = decodeURIComponent(url.pathname);
		if (rel.endsWith('/')) rel += 'index.html';
		const file = path.join(root, rel);
		if (!file.startsWith(root) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
			res.writeHead(404, { 'Content-Type': 'text/plain' });
			return res.end('not found');
		}
		const headers = { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream', 'Content-Length': fs.statSync(file).size };
		for (const rule of rules) {
			if (rule.re.test(url.pathname)) for (const h of rule.headers) headers[h.key] = h.value;
		}
		res.writeHead(200, headers);
		fs.createReadStream(file).pipe(res);
	});
	return new Promise((resolve) => server.listen(port, '127.0.0.1', () => resolve({ server, url: `http://127.0.0.1:${server.address().port}/` })));
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
	const root = process.argv[2] || 'dist';
	const port = Number(process.argv[3] || 8080);
	startServer(root, port).then(({ url }) => console.log(`serving ${root} at ${url}`));
}
