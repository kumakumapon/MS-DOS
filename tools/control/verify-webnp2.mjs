// Exercise CPANEL in the PC-98 CPU and verify its DOS configuration edits.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { resolve, join } from 'node:path';
import { mkdir, writeFile } from 'node:fs/promises';

const [checkout, base, output = 'tools/control/build/pc98-validation'] = process.argv.slice(2);
if (!checkout || !base) throw new Error('Usage: node verify-webnp2.mjs /path/to/webnp2 BASE_URL [output-dir]');
const require = createRequire(join(resolve(checkout), 'package.json'));
const browser = await require('puppeteer-core').launch({
  executablePath: process.env.CHROMIUM ?? '/usr/bin/chromium',
  headless: true,
  args: process.env.WEBNP2_NO_SANDBOX === '1' ? ['--no-sandbox'] : [],
});
const results = [];
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
try {
  await mkdir(output, { recursive: true });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto(base + '/?fd1=./test/dos4-control.xdf&run=1&worklet=0&lang=en&clk=8', { waitUntil: 'networkidle2' });
  await page.waitForFunction(() => window.np2debug?.np2?.isBooted());
  const screen = () => page.evaluate(() => window.np2debug.np2.getScreenText().text);
  const line = async needle => {
    await page.waitForFunction(
      needle => window.np2debug.np2.getScreenText().lines.some(text => text.trim() === needle),
      { timeout: 60000 }, needle,
    ).catch(async error => { console.error(await screen()); throw error; });
  };
  const key = async value => {
    await page.evaluate(value => window.np2debug.np2.typeText(value), value);
    await wait(250);
  };
  const command = async value => { await key(value + '\r'); };
  const guest = async name => Buffer.from(await page.evaluate(
    async name => btoa(String.fromCharCode(...await window.np2debug.np2.diskReadFile('fd1', name))), name,
  ), 'base64').toString('ascii');

  await line('A>');
  await command('CPANEL');
  await line('PC-98 SYSTEM SETTINGS');
  let text = await screen();
  assert.ok(text.includes('XMS manager FDXMS286.SYS : ENABLED'));
  assert.ok(text.includes('EMS manager EMM386.EXE   : ENABLED'));
  assert.ok(text.includes('FILES=20   BUFFERS=8'));
  results.push('CPANEL starts and reads the enabled XMS/EMS and DOS startup settings');

  await key('x');
  await key('=');
  await key(']');
  await key('f');
  await key('s');
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('Saved. Reboot'));
  const changedConfig = guest('CONFIG.SYS');
  assert.ok(changedConfig.includes('FILES=30\r\n'));
  assert.ok(changedConfig.includes('BUFFERS=16\r\n'));
  assert.ok(!changedConfig.includes('FDXMS286.SYS'));
  assert.ok(!changedConfig.includes('EMM386.EXE'));
  const changedAutoexec = guest('AUTOEXEC.BAT');
  assert.ok(changedAutoexec.includes('REM CPANEL AUTO FD98\r\nFD98\r\n'));
  results.push('Transactional save updates memory driver, FILES/BUFFERS and marked FD98 startup settings');

  await key('q');
  await line('A>');
  await command('CPANEL');
  await line('PC-98 SYSTEM SETTINGS');
  text = await screen();
  assert.ok(text.includes('XMS manager FDXMS286.SYS : DISABLED'));
  assert.ok(text.includes('EMS manager EMM386.EXE   : DISABLED'));
  assert.ok(text.includes('FILES=30   BUFFERS=16'));
  assert.ok(text.includes('Start FD98 automatically : YES'));
  await key('x');
  await key('e');
  await key('-');
  await key('[');
  await key('f');
  await key('s');
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('Saved. Reboot'));
  const restoredConfig = guest('CONFIG.SYS');
  assert.ok(restoredConfig.includes('FILES=20\r\n'));
  assert.ok(restoredConfig.includes('BUFFERS=8\r\n'));
  assert.ok(restoredConfig.includes('DEVICE=FDXMS286.SYS\r\n'));
  assert.ok(restoredConfig.includes('DEVICE=EMM386.EXE EMM=8192\r\n'));
  assert.ok(!guest('AUTOEXEC.BAT').includes('REM CPANEL AUTO FD98'));
  results.push('Reload detects saved settings and toggles return CONFIG.SYS/AUTOEXEC.BAT to their original options');

  await key('q');
  await line('A>');
  assert.deepEqual(errors, []);
  await page.screenshot({ path: join(output, 'control-panel.png') });
  await writeFile(join(output, 'verification.json'), JSON.stringify({ results, screen: await screen() }, null, 2) + '\n');
  console.log('PASS', results);
} finally {
  await browser.close();
}
