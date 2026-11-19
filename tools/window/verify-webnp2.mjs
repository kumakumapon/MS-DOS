// Usage: node tools/window/verify-webnp2.mjs WEBNP2_CHECKOUT [BASE_URL] [OUTPUT]
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { launchBrowser } from '../../ports/pc98/browser.mjs';
import { join } from 'node:path';
import { verifyWindow } from './verify-ui.mjs';
const [checkout, base = 'http://127.0.0.1:5173/', output = 'tools/window/build/pc98-validation'] = process.argv.slice(2);
if (!checkout) throw new Error('WEBNP2_CHECKOUT is required');
const browser = await launchBrowser(checkout);
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
  await page.evaluate(() => window.np2debug.np2.typeText('KDESK\r'));
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('Kuma Desktop'));
  await page.evaluate(() => window.np2debug.np2.sendKeys('F4'));
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('WINDOW MEMO OK'));
  results.push('saved desktop memo survives browser reload');
  await page.evaluate(() => window.np2debug.np2.sendKeys('F10'));
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().lines.some(s => s.trim() === 'A>'));
  await page.evaluate(() => window.np2debug.np2.typeText('ECHO JA>KUMAUI.CFG\rKDESK\r'));
  await page.waitForFunction(() => {
    const { base64 } = window.np2debug.np2.readMemoryBase64(0xa000c, 4);
    const pair = atob(base64);
    // First wide cell in "Kuma デスク", followed by its continuation cell.
    return pair.charCodeAt(0) === 5 && pair.charCodeAt(1) === 0xc7 &&
      pair.charCodeAt(2) === 0 && pair.charCodeAt(3) === 0;
  });
  await page.screenshot({ path: join(output, 'desktop-ja.png') });
  results.push('KUMAUI.CFG selects Shift-JIS desktop labels at launch');
  assert.deepEqual(errors, []);
  await writeFile(join(output, 'verification.json'), JSON.stringify({ browser: await browser.version(), results }, null, 2) + '\n');
  console.log('PASS:', results);
} finally { await browser.close(); }
