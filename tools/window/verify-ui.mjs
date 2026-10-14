// Exercise native PC-98 rendering, keyboard and bus-mouse input in WebNP2.
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
  const capture = async () => {
    if (!screenshot) return;
    // Help text is mirrored before painting finishes. The final task button
    // marks the end of the scene; also allow the next emulator video frame.
    await page.waitForFunction(() => window.np2debug.np2.getScreenText().lines[24]?.slice(65, 69) === 'Help',
      { timeout: 60000 });
    await new Promise(resolve => setTimeout(resolve, 100));
    await page.screenshot({ path: screenshot });
  };
  const selectFile = async name => {
    const names = await page.evaluate(async () =>
      (await window.np2debug.np2.diskListFiles('fd1')).entries.map(entry => entry.name));
    const index = names.indexOf(name);
    assert.ok(index >= 0, name + ' must be on the disk');
    await key('HOME');
    for (let i = 0; i < index; i++) await key('DOWN');
  };
  await type('MD GUIWORK\r');
  await type('KDESK\r'); await wait('Kuma Desktop'); await title('Launcher', 3, 5);
  const planes = await page.evaluate(() => [0xa8000, 0xb0000, 0xb8000, 0xe0000]
    .map(addr => window.np2debug.np2.readMemoryBase64(addr, 32000).base64));
  const point = (x, y) => planes.reduce((color, b64, i) => color |
    ((Buffer.from(b64, 'base64')[y * 80 + (x >> 3)] & (0x80 >> (x & 7))) ? 1 << i : 0), 0);
  assert.equal(point(0, 32), 1, 'Desktop is drawn to PC-98 graphics VRAM');
  assert.equal(point(24, 32), 7, 'Window frame is drawn to graphics VRAM');
  assert.equal(point(35, 48), 4, 'Active title bar has the focus color');
  await key('F6'); await key('RIGHT'); await key('DOWN'); await key('ENTER');
  await title('Launcher', 4, 6);
  await key('F7'); await key('RIGHT'); await key('DOWN'); await key('ENTER');
  await title('x', 4, 49);
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
  await capture();
  await key('ESC'); await key('F2'); await title('Launcher', 4, 6);
  await key('HOME'); await key('DOWN'); await key('DOWN'); await key('ENTER');
  await wait('PC-98 SYSTEM SETTINGS'); await type('q'); await wait('Press any key or click to return to KDESK');
  await key('SPACE'); await wait('Kuma Desktop'); await wait('Program returned to desktop');
  await key('F3'); await wait('Parent');
  await selectFile('GUIWORK'); await key('ENTER'); await wait('A:\\GUIWORK'); await wait('Empty directory');
  await key('BS');
  await selectFile('P98TEST.COM'); await key('ENTER'); await wait('DOS4 EXEC OK');
  await wait('Press any key or click to return to KDESK'); await key('SPACE'); await wait('Kuma Desktop');
  await selectFile('NOTE.TXT'); await key('ENTER'); await wait('EDIT98'); await wait('WINDOW MEMO OK');
  await key('F10'); await wait('Press any key or click to return to KDESK');
  await key('SPACE'); await wait('Kuma Desktop');
  await key('F10'); await wait('KDESK exited.');
  // Test a failed save against a real DOS-created conflicting temporary file.
  await type('ECHO RESERVED>KDTMP.TMP\r'); await type('KDESK\r'); await wait('Kuma Desktop');
  await key('F4'); await wait('WINDOW MEMO OK'); await type('X'); await key('CTRL+S');
  await wait('Cannot create KDTMP.TMP');
  assert.deepEqual(await guest('NOTE.TXT'), note, 'A failed save preserves the original file');
  await key('F10'); await wait('Unsaved memo:'); await key('ESC'); await wait('Close cancelled');
  await key('F10'); await wait('Unsaved memo:'); await type('d'); await wait('KDESK exited.');
  await type('DEL KDTMP.TMP\r');

  // The mouse suite starts a fresh desktop to give geometry stable coordinates.
  const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
  const move = async (x, y) => {
    await page.evaluate(({ x, y }) => window.np2debug.np2.mouseMoveTo(x, y), { x, y });
    await pause(250);
  };
  const click = async (x, y, count = 1) => {
    await move(x, y);
    await page.evaluate(count => window.np2debug.np2.mouseClick({ count }), count);
    await pause(250);
  };
  const cell = (x, y, count = 1) => click(x * 8 + 4, y * 16 + 8, count);
  const drag = async (from, to) => {
    await move(from.x, from.y);
    await page.evaluate(({ from, to }) => window.np2debug.np2.mouseDrag(from, to), { from, to });
    await pause(250);
  };
  const region = () => page.evaluate(() => [0xa8000, 0xb0000, 0xb8000, 0xe0000].map(
    addr => window.np2debug.np2.readMemoryBase64(addr + 354 * 80 + 74, 16 * 80).base64));
  await type('KDESK\r'); await title('Launcher', 3, 5);
  await page.evaluate(() => window.np2debug.np2.mouseHome()); await pause(250);
  const clean = await region();
  await move(605, 355);
  assert.notDeepEqual(await region(), clean, 'Pointer is rendered in graphics VRAM');
  await move(639, 399); await move(0, 0);
  assert.deepEqual(await region(), clean, 'Pointer restores the pixels underneath, including screen edges');
  await drag({ x: 100, y: 52 }, { x: 148, y: 84 });
  await title('Launcher', 5, 11);
  await drag({ x: 453, y: 317 }, { x: 485, y: 333 });
  await title('x', 5, 57); // 52 x 17 cells, moved to (9, 4).
  await cell(53, 5); await wait('Window minimized');
  assert.ok(!(await text()).includes('Launcher'));
  await cell(42, 24); await title('Launcher', 5, 11);
  await cell(57, 5);
  assert.ok(!(await text()).includes('Launcher'), 'Close button removes the window');
  await cell(41, 0); await title('Memo', 5, 13);
  await type('X'); await cell(55, 5); await wait('Unsaved memo:');
  await cell(53, 14); await wait('Close cancelled');
  assert.ok((await text()).includes('SECOND LINEX'));
  await cell(55, 5); await wait('Unsaved memo:');
  await cell(38, 14); await wait('Memo closed');
  await cell(41, 0); await wait('SECOND LINE');
  assert.ok(!(await text()).includes('SECOND LINEX'));
  await type('X'); await cell(55, 5); await wait('Unsaved memo:');
  await cell(24, 14); await wait('Memo closed');
  assert.deepEqual(await guest('NOTE.TXT'), Buffer.concat([note, Buffer.from('X')]));
  await cell(41, 0); await wait('SECOND LINEX'); await key('BS');
  await cell(15, 18); await wait('Saved NOTE.TXT');
  assert.deepEqual(await guest('NOTE.TXT'), note, 'Mouse Save uses the same safe save path');
  await cell(55, 5);
  await cell(24, 0); await title('Launcher', 5, 11);
  await cell(18, 11); await wait('PC-98 SYSTEM SETTINGS');
  await type('q'); await wait('Press any key or click to return to KDESK');
  await click(600, 350); await wait('Program returned to desktop');
  await cell(32, 0); await title('Files', 4, 9); await wait('Parent');
  const names = await page.evaluate(async () =>
    (await window.np2debug.np2.diskListFiles('fd1')).entries.map(entry => entry.name));
  const mouseSelect = async name => {
    const index = names.indexOf(name);
    assert.ok(index >= 0, name + ' must be on disk');
    // Scroll buttons change selection and keep its row visible.
    await key('HOME');
    for (let i = 0; i < index; i++) await cell(52, 15);
    const row = 8 + Math.min(index, 8);
    await page.waitForFunction(({ name, row }) => window.np2debug.np2.getScreenText().lines[row]?.includes(name),
      { timeout: 60000 }, { name, row }).catch(async error => { console.error(await text()); throw error; });
    return row;
  };
  let row = await mouseSelect('GUIWORK');
  await cell(18, row, 2); await wait('A:\\GUIWORK'); await wait('Empty directory');
  await cell(11, 17); await wait('A:\\');
  row = await mouseSelect('P98TEST.COM'); await cell(18, row);
  await cell(21, 17); await wait('DOS4 EXEC OK');
  await wait('Press any key or click to return to KDESK');
  await click(600, 350); await wait('Program returned to desktop');
  // The desktop mouse still works after two child EXEC/return cycles.
  await cell(49, 0); await wait('Drag title: move');
  await capture();
  await cell(73, 0); await wait('KDESK exited.');
  return [
    'KDESK 16-color graphics, CG-ROM labels, keyboard window move and resize',
    'overlapping windows, focus, minimize/restore, close and help',
    'memo CRLF save, reopen, unsaved-change cancel/discard and failed-save preservation',
    'directory navigation; CPANEL, child COM and EDIT98 launch/return; DOS exit',
    'bus-mouse pointer restoration/edge clipping; title drag, resize, close and taskbar restore',
    'mouse memo Save/Cancel/Discard, launcher, file scrolling/double-click/Open/Parent and child return',
  ];
}
