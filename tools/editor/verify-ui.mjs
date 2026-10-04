// Exercise the native editor and console through guest keys; read files only for assertions.
import assert from 'node:assert/strict';

export async function verifyEditor(page, { screenshot } = {}) {
  const screen = () => page.evaluate(() => window.np2debug.np2.getScreenText().text);
  const wait = async needle => page.waitForFunction(
    needle => window.np2debug?.np2?.getScreenText().text.includes(needle),
    { timeout: 30000 }, needle,
  ).catch(async error => { console.error(await screen()); throw error; });
  const key = async value => page.evaluate(value => window.np2debug.np2.sendKeys(value), value);
  const type = async value => page.evaluate(value => window.np2debug.np2.typeText(value), value);
  const paste = async value => page.evaluate(value => window.np2debug.np2.pasteText(value), value);
  const guest = async name => Buffer.from(await page.evaluate(
    async name => btoa(String.fromCharCode(...await window.np2debug.np2.diskReadFile('fd1', name))), name,
  ), 'base64');
  await type('CLS\rTYPE JPHELLO.TXT\r'); await wait('日本語表示のテスト');
  await wait('漢字・ひらがな・カタカナ・ｶﾅ');
  if (screenshot) await page.screenshot({ path: screenshot.replace('.png', '-console.png') });
  await type('EDIT JPTEST.TXT\r'); await wait('EDIT98 - Shift_JIS');
  await paste('日本語\nｶﾅ ABC'); await wait('日本語'); await wait('ｶﾅ ABC');
  await key('HOME'); await key('DEL'); await wait('ﾅ ABC');
  await key('UP'); await key('RIGHT'); await key('DEL'); await wait('日語');
  await key('CTRL+Z'); await wait('日本語');
  await key('RIGHT'); await key('BS'); await wait('日語');
  await paste('本'); await wait('日本語');
  await key('F3'); await wait('Saved (Shift_JIS / CRLF)');
  const expected = Buffer.from([0x93,0xfa,0x96,0x7b,0x8c,0xea,13,10,0xc5,32,65,66,67]);
  assert.deepEqual(await guest('JPTEST.TXT'), expected);
  await key('F10'); await wait('EDIT98 exited.');
  await type('EDIT98 JPTEST.TXT\r'); await wait('EDIT98 - Shift_JIS'); await wait('日本語');
  await key('F5'); await wait('Find (Shift_JIS)'); await paste('語'); await key('ENTER'); await wait('Found');
  if (screenshot) await page.screenshot({ path: screenshot });
  await paste('X'); await key('F10'); await wait('Unsaved changes'); await key('ESC');
  await wait('EDIT98 - Shift_JIS'); assert.deepEqual(await guest('JPTEST.TXT'), expected);
  await key('F10'); await wait('Unsaved changes'); await key('N'); await wait('EDIT98 exited.');
  await type('CLS\rTYPE JPTEST.TXT\r'); await wait('日本語'); await wait('ﾅ ABC');
  assert.deepEqual(await guest('JPTEST.TXT'), expected);
  // Actual console wrap + scroll: the pair at column 79 must start on a new line.
  for (let i = 0; i < 25; i++) await paste('ECHO ' + 'A'.repeat(79) + '日本語\n');
  await wait('日本語');
  const lines = (await screen()).split('\n');
  assert.ok(lines.some(line => line.startsWith('日本語')), 'Full-width wrap survives console scroll');
  await type('FD98\r'); await wait('FD Filer 1.0'); await key('HOME');
  for (let i = 0; i < 256; i++) {
    if ((await screen()).split('\n')[22]?.includes('Selected: JPTEST.TXT')) break;
    if (i === 255) throw new Error('Cannot select JPTEST.TXT');
    await key('DOWN');
  }
  await key('F3'); await wait('FD Filer - Viewer'); await wait('日本語'); await key('ESC');
  await type('E'); await wait('EDIT98 - Shift_JIS'); await wait('日本語');
  await key('F10'); await wait('Program returned. Press any key'); await key('SPACE');
  await wait('FD Filer 1.0'); await key('F10'); await wait('FD Filer exited.');
  return ['native Japanese CON/TYPE, kana, full-width wrap and scrolling',
    'EDIT/EDIT98 new/open, pair-aware movement/delete/backspace, undo, search and CRLF save',
    'byte-exact Shift_JIS guest file, reopen, unsaved-change cancel/discard and FD98 E launch'];
}
