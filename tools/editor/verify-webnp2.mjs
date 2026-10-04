// Usage: node tools/editor/verify-webnp2.mjs WEBNP2_CHECKOUT [BASE_URL] [OUTPUT]
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { join, resolve } from 'node:path';
import { verifyEditor } from './verify-ui.mjs';
const [checkout, base = 'http://127.0.0.1:5173/', output = 'tools/editor/build/pc98-validation'] = process.argv.slice(2);
if (!checkout) throw new Error('WEBNP2_CHECKOUT is required');
const require = createRequire(join(resolve(checkout), 'package.json'));
const browser = await require('puppeteer-core').launch({
  executablePath: process.env.CHROMIUM ?? '/usr/bin/chromium', headless: true,
  args: process.env.WEBNP2_NO_SANDBOX === '1' ? ['--no-sandbox'] : [],
});
try {
  await mkdir(output, { recursive: true });
  const page = await browser.newPage(), errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(base + '?fd1=./test/dos4-editor.xdf&run=1&clk=8&worklet=0', { waitUntil: 'networkidle2' });
  await page.waitForFunction(() => window.np2debug?.np2?.getScreenText().lines.some(s => s.trim() === 'A>'));
  const results = await verifyEditor(page, { screenshot: join(output, 'editor.png') });
  assert.deepEqual(errors, []);
  await writeFile(join(output, 'verification.json'), JSON.stringify({ browser: await browser.version(), results }, null, 2) + '\n');
  console.log('PASS:', results);
} finally { await browser.close(); }
