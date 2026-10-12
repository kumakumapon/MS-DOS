// Usage: node tools/window/verify-webnp2.mjs WEBNP2_CHECKOUT [BASE_URL] [OUTPUT]
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { join, resolve } from 'node:path';
import { verifyWindow } from './verify-ui.mjs';
const [checkout, base = 'http://127.0.0.1:5173/', output = 'tools/window/build/pc98-validation'] = process.argv.slice(2);
if (!checkout) throw new Error('WEBNP2_CHECKOUT is required');
const require = createRequire(join(resolve(checkout), 'package.json'));
const browser = await require('puppeteer-core').launch({
  executablePath: process.env.CHROMIUM ?? '/usr/bin/chromium', headless: true,
  args: process.env.WEBNP2_NO_SANDBOX === '1' ? ['--no-sandbox'] : [],
});
try {
  await mkdir(output, { recursive: true });
  const page = await browser.newPage(); const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(new URL('?fd1=./test/dos4-window.xdf&run=1&clk=8&worklet=0', base).href,
    { waitUntil: 'networkidle2' });
  await page.waitForFunction(() => window.np2debug?.np2?.getScreenText().lines.some(s => s.trim() === 'A>'), { timeout: 60000 });
  const results = await verifyWindow(page, { screenshot: join(output, 'desktop.png') });
  await page.evaluate(() => window.np2debug.np2.persistNow({ force: true }));
  await page.reload({ waitUntil: 'networkidle2' });
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().lines.some(s => s.trim() === 'A>'), { timeout: 60000 });
  await page.evaluate(() => window.np2debug.np2.typeText('WIN98\r'));
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('WIN98 Desktop'));
  await page.evaluate(() => window.np2debug.np2.sendKeys('F4'));
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('WINDOW MEMO OK'));
  results.push('saved desktop memo survives browser reload');
  assert.deepEqual(errors, []);
  await writeFile(join(output, 'verification.json'), JSON.stringify({ browser: await browser.version(), results }, null, 2) + '\n');
  console.log('PASS:', results);
} finally { await browser.close(); }
