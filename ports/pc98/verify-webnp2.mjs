// Real browser/CPU validation; run against a local, unmodified WebNP2 checkout.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { resolve, join } from 'node:path';
import { mkdir, writeFile, readFile as readHostFile } from 'node:fs/promises';
const [checkout, imageName, output = 'ports/pc98/build/validation'] = process.argv.slice(2);
if (!checkout || !/^[\w.-]+\.xdf$/.test(imageName ?? '')) {
  throw new Error('Usage: node verify-webnp2.mjs /path/to/webnp2 image.xdf [output-dir]');
}
const require = createRequire(join(resolve(checkout), 'package.json'));
const browser = await require('puppeteer-core').launch({
  executablePath: process.env.CHROMIUM ?? '/usr/bin/chromium',
  headless: true,
  protocolTimeout: 660000,
  args: process.env.WEBNP2_NO_SANDBOX === '1' ? ['--no-sandbox'] : [],
});
const results = [];
const sourceImage = await readHostFile(join(resolve(checkout), 'public/test', imageName));
const imageSha256 = createHash('sha256').update(sourceImage).digest('hex');
const webnp2Commit = execFileSync('git', ['-C', resolve(checkout), 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
const base = process.env.WEBNP2_URL ?? 'http://127.0.0.1:5173';
const dosVersion = process.env.DOS_VERSION ?? '2';
assert.ok(['2', '4'].includes(dosVersion), 'DOS_VERSION must be 2 or 4');
const probe = `DOS${dosVersion}-WRITE-READ`;
await mkdir(output, { recursive: true });
function readFile(image, name) {
  const fat = image.subarray(1024, 3072), root = image.subarray(5120, 11264);
  assert.deepEqual(fat, image.subarray(3072, 5120), 'FAT copies differ');
  const [stem, ext] = name.split('.');
  const target = stem.padEnd(8) + ext.padEnd(3);
  let entry;
  for (let i = 0; i < root.length; i += 32) {
    if (root.subarray(i, i + 11).toString('ascii') === target) { entry = root.subarray(i, i + 32); break; }
  }
  assert.ok(entry, `Missing ${name}`);
  let cluster = entry.readUInt16LE(26), size = entry.readUInt32LE(28);
  const blocks = [], seen = new Set();
  while (size > 0) {
    assert.ok(cluster >= 2 && cluster < 1223 && !seen.has(cluster), 'Bad FAT chain');
    seen.add(cluster);
    const offset = (11 + cluster - 2) * 1024;
    blocks.push(image.subarray(offset, offset + Math.min(size, 1024)));
    size -= 1024;
    const value = fat.readUInt16LE(Math.floor(cluster * 3 / 2));
    cluster = cluster & 1 ? value >> 4 : value & 0xfff;
  }
  return Buffer.concat(blocks);
}
try {
  const page = await browser.newPage();
  const pageErrors = [];
  page.on('pageerror', e => pageErrors.push(e.message));
  await page.goto(`${base}/?fd1=./test/${imageName}&run=1&worklet=0&lang=en&clk=8`, { waitUntil: 'networkidle2' });
  await page.waitForFunction(() => window.np2debug?.np2?.isBooted());
  const text = () => page.evaluate(() => window.np2debug.np2.getScreenText().text);
  const command = async value => {
    await page.evaluate(async value => window.np2debug.np2.typeText(value + '\r'), value);
    await new Promise(resolve => setTimeout(resolve, 250));
  };
  const line = async (needle, timeout=60000) => {
    await page.waitForFunction(needle => window.np2debug.np2.getScreenText().lines.some(s => s.trim() === needle), { timeout }, needle).catch(async e=>{console.log('TIMEOUT',await text());console.log(await page.evaluate(()=>window.np2debug.np2.dbgReadRegs()));throw e;});
  };
  await line('A>');
  if (dosVersion === '2') assert.ok((await text()).includes('MS-DOS version 2.00'));
  assert.ok(!(await text()).includes('Specified COMMAND search directory bad'));
  results.push(dosVersion === '2' ? 'Microsoft MS-DOS 2.00 kernel and bundled Command 2.02 boot' : 'source-built Microsoft MS-DOS 4.00 and COMMAND.COM boot');
  await command('VER');
  await line(dosVersion === '2' ? 'MS-DOS Version  2.00' : 'MS-DOS Version 4.00');
  await command(`ECHO ${probe}>PROBE.TXT`);
  await command('TYPE PROBE.TXT');
  await line(probe);
  await command('COPY PROBE.TXT COPIED.TXT');
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().lines.some(s => /^1\s+file\(s\) copied\.?$/i.test(s.trim())), { timeout: 10000 }).catch(async e => { console.log(await text()); throw e; });
  results.push('DOS VER, ECHO redirection, TYPE and COPY');
  await command('DATE');
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().text.includes('Enter new date'), { timeout: 10000 });
  const currentDate = (await text()).split('\n').find(s => s.includes('Current date is'));
  assert.ok(currentDate?.includes(String(new Date().getFullYear())), 'RTC year must match the emulator host');
  await command('');
  results.push('PC-98 RTC calendar read through DOS DATE');
  if (dosVersion === '4') {
    await command('P98TEST > EXEC.TXT');
    await command('TYPE EXEC.TXT');
    await line('DOS4 EXEC OK');
    await command('ECHO DOS4-RETURN-OK');
    await line('DOS4-RETURN-OK');
    results.push('COM EXEC, DOS version API 4.00, redirected output and INT 21h/4Ch return');
  }
  if (process.env.RETROBASIC === '1') {
    await command('RBASIC');
    await line('Ready');
    for (const value of ['10 FOR I=1 TO 3', '20 PRINT I', '30 NEXT I', '40 PRINT "NATIVE OUTPUT OK"', 'RUN']) await command(value);
    await line('NATIVE OUTPUT OK');
    await command('SAVE "PROOF.BAS"');
    await command('NEW');
    await command('LOAD "PROOF.BAS"');
    await command('CLS 1');
    await command('RUN');
    await line('NATIVE OUTPUT OK');
    await command('SYSTEM');
    await command('ECHO DOS-RETURN-OK');
    await line('DOS-RETURN-OK');
    results.push('native RetroBasic REPL, FOR/NEXT, RUN, SAVE/NEW/LOAD and SYSTEM');
    for (const [file, expected] of [
      ['PRIMES.BAS', 'Total primes found:  46'],
      ['FUNCTEST.BAS', 'All Tests Completed Successfully!'],
      ['GRAPHICS.BAS', 'Graphic demo rendered successfully.'],
      ['WAVE3D.BAS', '3D Wave rendering complete.'],
      [process.env.RETROBASIC_SLOW === '1' ? 'MANDEL.BAS' : 'MANSMOKE.BAS', 'Mandelbrot generation complete.'],
    ]) {
      console.log(`Running ${file}`);
      await command('RBASIC ' + file);
      await line(expected,file==='MANDEL.BAS'?600000:60000);
      if (file === 'GRAPHICS.BAS') {
        const planes = await page.evaluate(() => [0xa8000, 0xb0000, 0xb8000].map(addr => window.np2debug.np2.readMemoryBase64(addr, 32000).base64));
        const point = (x, y) => planes.reduce((c, b64, i) => c | ((Buffer.from(b64, 'base64')[y * 80 + (x >> 3)] & (0x80 >> (x & 7))) ? 1 << i : 0), 0);
        assert.equal(point(70, 60), 1, 'blue rectangle in actual PC-98 VRAM');
        assert.equal(point(535, 220), 4, 'green flood fill in actual PC-98 VRAM');
        await page.screenshot({ path: join(output, 'graphics.png') });
      }
      results.push(`native sample ${file}${file==='MANSMOKE.BAS'?' (STEP 32; full-resolution original kept as MANDEL.BAS)':''}`);
    }
    if (process.env.RETROBASIC_FILES === '1') {
      await command('RBASIC FILEIO.BAS');
      await line('NATIVE FILE IO OK');
      results.push('native OPEN INPUT/OUTPUT/APPEND, WRITE#/PRINT#/INPUT#/LINE INPUT#, EOF/LOF/LOC/INPUT$ and CLOSE');
    }
  }
  const exported = await page.evaluate(async () => window.np2debug.np2.exportDiskBase64('fd1'));
  const image = Buffer.from(exported.base64, 'base64');
  assert.equal(readFile(image, 'PROBE.TXT').toString('ascii').trim(), probe);
  assert.deepEqual(readFile(image, 'PROBE.TXT'), readFile(image, 'COPIED.TXT'));
  if (dosVersion === '4') assert.equal(readFile(image, 'EXEC.TXT').toString('ascii'), 'DOS4 EXEC OK\r\n');
  if (process.env.RETROBASIC === '1') {
    assert.equal(readFile(image, 'PROOF.BAS').toString('ascii'), '10 FOR I=1 TO 3\r\n20 PRINT I\r\n30 NEXT I\r\n40 PRINT "NATIVE OUTPUT OK"\r\n');
    if (process.env.RETROBASIC_FILES === '1')
      assert.equal(readFile(image, 'SEQ.TXT').toString('ascii'), '"A,B",42,"Q""R"\r\nTAIL\r\nAPPEND\r\n');
  }
  results.push('readback of guest-written FAT12 files and both FAT copies');
  assert.deepEqual(pageErrors, [], 'WebNP2 page errors');
  await writeFile(join(output, 'guest-written.xdf'), image);
  await page.screenshot({ path: join(output, 'screen.png') });
  await writeFile(join(output, 'verification.json'), JSON.stringify({ browser: await browser.version(), dos_version: dosVersion, image_sha256: imageSha256, webnp2_commit: webnp2Commit, cpu_clock_multiplier: 8, results, screen: await text() }, null, 2) + '\n');
  console.log('PASS', results);
} finally { await browser.close(); }
