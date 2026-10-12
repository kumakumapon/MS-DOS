// Exercise the guest through DOS commands and real keyboard input.
import assert from 'node:assert/strict';

export async function verifyWindow(page, { screenshot } = {}) {
  const text = () => page.evaluate(() => window.np2debug.np2.getScreenText().text);
  const wait = needle => page.waitForFunction(
    needle => window.np2debug?.np2?.getScreenText().text.includes(needle),
    { timeout: 60000 }, needle,
  ).catch(async error => { console.error(await text()); throw error; });
  const key = async value => {
    await page.evaluate(value => window.np2debug.np2.sendKeys(value), value);
    await new Promise(resolve => setTimeout(resolve, 250));
  };
  const type = async value => {
    await page.evaluate(value => window.np2debug.np2.typeText(value), value);
    await new Promise(resolve => setTimeout(resolve, 250));
  };
  const guest = async name => Buffer.from(await page.evaluate(async name =>
    btoa(String.fromCharCode(...await window.np2debug.np2.diskReadFile('fd1', name))), name), 'base64');
  const title = async (name, row, column) => page.waitForFunction(
    ({ name, row, column }) => window.np2debug.np2.getScreenText().lines[row]?.indexOf(name) === column,
    { timeout: 60000 }, { name, row, column },
  );
  const selectFile = async name => {
    const names = await page.evaluate(async () =>
      (await window.np2debug.np2.diskListFiles('fd1')).entries.map(entry => entry.name));
    const index = names.indexOf(name);
    assert.ok(index >= 0, name + ' must be on the disk');
    await key('HOME');
    for (let i = 0; i < index; i++) await key('DOWN');
  };
  await type('MD GUIWORK\r');
  await type('WIN98\r'); await wait('WIN98 Desktop'); await title('Launcher', 3, 5);
  const planes = await page.evaluate(() => [0xa8000, 0xb0000, 0xb8000]
    .map(addr => window.np2debug.np2.readMemoryBase64(addr, 32000).base64));
  const point = (x, y) => planes.reduce((color, b64, i) => color |
    ((Buffer.from(b64, 'base64')[y * 80 + (x >> 3)] & (0x80 >> (x & 7))) ? 1 << i : 0), 0);
  assert.equal(point(0, 32), 1, 'Desktop is drawn to PC-98 graphics VRAM');
  assert.equal(point(24, 32), 7, 'Window frame is drawn to graphics VRAM');
  assert.equal(point(40, 48), 5, 'Active title bar has the focus color');
  await key('F6'); await key('RIGHT'); await key('DOWN'); await key('ENTER');
  await title('Launcher', 4, 6);
  await key('F7'); await key('RIGHT'); await key('DOWN'); await key('ENTER');
  await title('[Esc]', 4, 47);
  await key('F4'); await wait('NOTE.TXT');
  await type('WINDOW MEMO OK\rSECOND LINE'); await key('CTRL+S'); await wait('Saved NOTE.TXT');
  const note = Buffer.from('WINDOW MEMO OK\r\nSECOND LINE');
  assert.deepEqual(await guest('NOTE.TXT'), note);
  await type('X'); await key('ESC'); await wait('Unsaved memo:');
  await key('ESC'); await wait('Close cancelled');
  await key('ESC'); await type('d'); await wait('Memo closed');
  await key('F4'); await wait('WINDOW MEMO OK');
  assert.ok(!(await text()).includes('SECOND LINEX'), 'Discard reloads the saved note');
  await key('F1'); await wait('F6 then arrows: move window');
  await key('F8'); await wait('Window minimized');
  assert.ok(!(await text()).includes('F6 then arrows: move window'));
  await key('TAB'); await wait('F6 then arrows: move window');
  if (screenshot) await page.screenshot({ path: screenshot });
  await key('ESC'); await key('F2'); await title('Launcher', 4, 6);
  await key('HOME'); await key('DOWN'); await key('DOWN'); await key('ENTER');
  await wait('PC-98 SYSTEM SETTINGS'); await type('q'); await wait('Press any key to return to WIN98');
  await key('SPACE'); await wait('WIN98 Desktop'); await wait('Program returned to desktop');
  await key('F3'); await wait('Backspace: parent');
  await selectFile('GUIWORK'); await key('ENTER'); await wait('A:\\GUIWORK'); await wait('Empty directory');
  await key('BS');
  await selectFile('P98TEST.COM'); await key('ENTER'); await wait('DOS4 EXEC OK');
  await wait('Press any key to return to WIN98'); await key('SPACE'); await wait('WIN98 Desktop');
  await selectFile('NOTE.TXT'); await key('ENTER'); await wait('EDIT98'); await wait('WINDOW MEMO OK');
  await key('F10'); await wait('Press any key to return to WIN98');
  await key('SPACE'); await wait('WIN98 Desktop');
  await key('F10'); await wait('WIN98 exited.');
  // Test a failed save against a real DOS-created conflicting temporary file.
  await type('ECHO RESERVED>W98TMP.TMP\r'); await type('WIN98\r'); await wait('WIN98 Desktop');
  await key('F4'); await wait('WINDOW MEMO OK'); await type('X'); await key('CTRL+S');
  await wait('Cannot create W98TMP.TMP');
  assert.deepEqual(await guest('NOTE.TXT'), note, 'A failed save preserves the original file');
  await key('F10'); await wait('Unsaved memo:'); await key('ESC'); await wait('Close cancelled');
  await key('F10'); await wait('Unsaved memo:'); await type('d'); await wait('WIN98 exited.');
  await type('DEL W98TMP.TMP\r');
  return [
    'WIN98 real PC-98 graphics planes, native text, window move and resize',
    'overlapping windows, focus, minimize/restore, close and help',
    'memo CRLF save, reopen, unsaved-change cancel/discard and failed-save preservation',
    'directory navigation; CPANEL, child COM and EDIT98 launch/return; DOS exit',
  ];
}
