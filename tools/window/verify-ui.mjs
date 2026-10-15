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
  await wait('Ready');
  const point = (x, y) => page.evaluate(({ x, y }) => {
    const source = document.querySelector('#canvas'), canvas = document.createElement('canvas');
    canvas.width = source.width; canvas.height = source.height;
    const ctx = canvas.getContext('2d'); ctx.drawImage(source, 0, 0);
    return Array.from(ctx.getImageData(x, y, 1, 1).data).slice(0, 3);
  }, { x, y });
  assert.ok((await point(0,32)).every((c,i) => Math.abs(c-[25,66,90][i])<=4), 'Desktop uses the PEGC gradient palette (RGB565 output tolerance)');
  assert.ok((await point(24,32)).every(c => Math.abs(c-255)<=4), 'Window frame is drawn by the guest');
  assert.ok((await point(35,48)).every((c,i) => Math.abs(c-[34,119,187][i])<=4), 'Active title bar has the focus color');
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
  const region = () => page.evaluate(() => {
    const source = document.querySelector('#canvas'), canvas = document.createElement('canvas');
    canvas.width = source.width; canvas.height = source.height;
    const ctx = canvas.getContext('2d'); ctx.drawImage(source, 0, 0);
    return Array.from(ctx.getImageData(592, 350, 48, 32).data);
  });
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
  const mediaResults = await verifyMedia(page, { screenshot: screenshot?.replace(/\.png$/, '-media.png') });
  return [
    ...mediaResults,
    'KDESK PEGC 256-color graphics, CG-ROM labels, keyboard window move and resize',
    'overlapping windows, focus, minimize/restore, close and help',
    'memo CRLF save, reopen, unsaved-change cancel/discard and failed-save preservation',
    'directory navigation; CPANEL, child COM and EDIT98 launch/return; DOS exit',
    'bus-mouse pointer restoration/edge clipping; title drag, resize, close and taskbar restore',
    'mouse memo Save/Cancel/Discard, launcher, file scrolling/double-click/Open/Parent and child return',
  ];
}

export async function verifyMedia(page, { screenshot } = {}) {
  const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
  const text = () => page.evaluate(() => window.np2debug.np2.getScreenText().text);
  const wait = needle => page.waitForFunction(needle => window.np2debug.np2.getScreenText().text.includes(needle),
    { timeout: 60000 }, needle).catch(async e => { console.error(await text()); throw e; });
  const key = async value => { await page.evaluate(v => window.np2debug.np2.sendKeys(v), value); await pause(250); };
  const type = async value => { await page.evaluate(v => window.np2debug.np2.typeText(v), value); await pause(250); };
  const move = async (x,y) => { await page.evaluate(({x,y}) => window.np2debug.np2.mouseMoveTo(x,y), {x,y}); await pause(200); };
  const click = async (x,y) => { await move(x,y); await page.evaluate(() => window.np2debug.np2.mouseClick()); await pause(250); };
  const drag = async (from,to) => { await move(from.x,from.y); await page.evaluate(({from,to}) => window.np2debug.np2.mouseDrag(from,to), {from,to}); await pause(250); };
  const guest = async name => Buffer.from(await page.evaluate(async name => Array.from(await window.np2debug.np2.diskReadFile('fd1',name)),name));
  const mailbox = active => page.evaluate(active => {
    const np2 = window.np2debug.np2, start = 0x1000;
    const bytes = atob(np2.readMemoryBase64(start,0x9f000).base64);
    let at = -1;
    while ((at = bytes.indexOf('KDESKCLP',at+1)) >= 0) {
      const word = offset => bytes.charCodeAt(at+offset) | bytes.charCodeAt(at+offset+1)<<8;
      if (word(8) === 1 && word(10) === active) return {
        address:start+at, text:bytes.slice(at+30,at+30+word(12)), width:word(22), height:word(24),segment:word(26),
      };
    }
    return null;
  },active);
  await type('KDESK\r'); await wait('Ready'); await key('F4'); await wait('NOTE.TXT');
  await key('CTRL+A'); await key('CTRL+C'); await wait('Copied memo');
  assert.equal((await mailbox(1)).text, 'WINDOW MEMO OK\nSECOND LINE');
  await key('CTRL+X'); await wait('Cut memo'); await key('CTRL+V'); await wait('Pasted clipboard');
  await key('CTRL+S'); await wait('Saved NOTE.TXT');
  assert.equal((await guest('NOTE.TXT')).toString(),'WINDOW MEMO OK\r\nSECOND LINE');
  await key('F2'); await wait('Click an app'); await page.evaluate(() => window.np2debug.np2.mouseHome()); await pause(500);
  await move(100,52);
  await page.evaluate(() => {
    const np2 = window.np2debug.np2, original = np2.mouseMoveTo.bind(np2);
    window.mediaOriginalMove = original;
    np2.mouseMoveTo = async (x,y) => { const value = await original(x,y); if (x===151 && y===87) await new Promise(resolve => {window.mediaRelease=resolve;}); return value; };
    window.mediaDrag = np2.mouseDrag({x:100,y:52},{x:151,y:87});
  });
  await page.waitForFunction(() => !!window.mediaRelease);
  await page.waitForFunction(() => {
    const source=document.querySelector('#canvas'),c=document.createElement('canvas');c.width=source.width;c.height=source.height;
    const g=c.getContext('2d');g.drawImage(source,0,0);const p=g.getImageData(84,90,1,1).data;
    return [34,119,187].every((v,i)=>Math.abs(p[i]-v)<=4);
  },{timeout:60000});
  const livePixel = await page.evaluate(() => {
    const source=document.querySelector('#canvas'), c=document.createElement('canvas'); c.width=source.width;c.height=source.height;
    const g=c.getContext('2d');g.drawImage(source,0,0);return Array.from(g.getImageData(84,90,1,1).data).slice(0,3);
  });
  assert.ok(livePixel.every((c,i) => Math.abs(c-[34,119,187][i])<=4),'Window contents move at pixel resolution while the button remains held: '+livePixel);
  await page.evaluate(async () => { window.mediaRelease(); await window.mediaDrag; window.np2debug.np2.mouseMoveTo=window.mediaOriginalMove; });
  await key('F10'); await wait('KDESK exited.');
  await type('ECHO NOTBMP>BAD.BMP\r'); await type('KVIEW COLORS.BMP\r'); await wait('256 colors'); await wait('Opened BMP');
  await pause(150);
  const paletteColors = await page.evaluate(() => {
    const source=document.querySelector('#canvas'),c=document.createElement('canvas');c.width=source.width;c.height=source.height;
    const g=c.getContext('2d');g.drawImage(source,0,0);const colors=new Set();
    for(let i=0;i<256;i++) colors.add(Array.from(g.getImageData(515+i%16*7,88+Math.floor(i/16)*16,1,1).data).slice(0,3).join(','));
    return colors.size;
  });
  assert.ok(paletteColors>200,'More than 200 distinct RGB colors are visible simultaneously');
  await key('CTRL+C'); await wait('Copied image');
  assert.deepEqual([...(b=>[b.width,b.height])(await mailbox(3))],[160,112]);
  if(screenshot) await page.screenshot({path:screenshot.replace(/\.png$/,'-viewer.png')});
  await key('F10'); await wait('KVIEW exited.');
  // Odd widths exercise row padding; top-down 24-bit data also checks orientation.
  const fixtures = [1,4,24].map(bits => {
    const palette = [[255,0,0],[0,0,255],[0,255,0],[0,0,0]].slice(0,bits===1?2:4);
    const indices = bits===1 ? [0,1,0,1,0,1] : [0,1,2,3,2,1];
    const stride = Math.ceil(3*bits/32)*4, offset = 54+(bits===24?0:palette.length*4);
    const bmp = Buffer.alloc(offset+stride*2); bmp.write('BM');bmp.writeUInt32LE(bmp.length,2);bmp.writeUInt32LE(offset,10);
    bmp.writeUInt32LE(40,14);bmp.writeInt32LE(3,18);bmp.writeInt32LE(bits===24?-2:2,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(bits,28);
    if(bits!==24) {bmp.writeUInt32LE(palette.length,46);palette.forEach(([r,g,b],i)=>bmp.set([b,g,r,0],54+i*4));}
    for(let y=0;y<2;y++) for(let x=0;x<3;x++) {
      const i=indices[y*3+x], at=offset+(bits===24?y:1-y)*stride;
      if(bits===24) {const [r,g,b]=palette[i];bmp.set([b,g,r],at+x*3);}
      else if(bits===4) bmp[at+(x>>1)] |= i << (x%2?0:4);
      else bmp[at] |= i << (7-x);
    }
    return {name:`B${bits}.BMP`,bytes:Array.from(bmp),expected:indices.map(i=>[196,21,46,16][i])};
  });
  for(const fixture of fixtures) {
    await page.evaluate(async ({name,bytes})=>window.np2debug.np2.diskWriteFile('fd1',name,new Uint8Array(bytes)),fixture);
    await type(`KVIEW ${fixture.name}\r`);await wait('Opened BMP');
    const image=await mailbox(3);assert.deepEqual([image.width,image.height],[3,2]);
    const pixels=await page.evaluate(segment=>Array.from(atob(window.np2debug.np2.readMemoryBase64(segment*16,6).base64),c=>c.charCodeAt(0)),image.segment);
    assert.deepEqual(pixels,fixture.expected,`${fixture.name}: palette, row padding and orientation`);
    await key('F10');await wait('KVIEW exited.');
  }
  await type('KPAINT\r'); await wait('Ready'); await page.evaluate(() => window.np2debug.np2.mouseHome()); await pause(500);
  await drag({x:36,y:100},{x:96,y:140}); await wait('Image changed'); await key('F3'); await wait('Saved BMP');
  const pixel=(bmp,x,y)=>bmp[1078+(bmp.readInt32LE(22)-y-1)*((bmp.readInt32LE(18)+3)&~3)+x];
  // Absolute bus-mouse coordinates are host estimates; allow a two-pixel
  // delivery offset while checking several points along the interpolated stroke.
  const near=(bmp,x,y,c)=>[-2,-1,0,1,2].some(dy=>[-2,-1,0,1,2].some(dx=>pixel(bmp,x+dx,y+dy)===c));
  let bmp=await guest('DRAW.BMP'); assert.equal(bmp.readUInt16LE(28),8);assert.equal(bmp.readInt32LE(18),224);assert.equal(bmp.length,33334);
  for(const [x,y] of [[20,17],[25,20],[30,23]]) assert.ok(near(bmp,x,y,196),'Pencil interpolates the drag path');
  await key('CTRL+Z'); await wait('Undo / redo'); await key('F3'); await wait('Saved BMP');
  assert.ok(!near(await guest('DRAW.BMP'),25,20,196),'Undo restores the original pixels');
  await key('CTRL+Z'); await wait('Undo / redo'); await key('s'); await wait('Tool: Select');
  await drag({x:32,y:96},{x:100,y:144}); await wait('Selection ready'); await key('CTRL+C'); await wait('Copied image');
  const clip=await mailbox(4);assert.deepEqual([clip.width,clip.height],[35,25]);
  await move(136,160); await key('CTRL+V'); await wait('Pasted image');await key('F3');await wait('Saved BMP');
  assert.ok(near(await guest('DRAW.BMP'),77,52,196),'Image selection pastes at the cursor');
  await key('F5');await wait('New image');await key('r');await wait('Tool: Rect');
  await drag({x:26,y:90},{x:56,y:120});await wait('Image changed');
  await click(550,104);await key('f');await wait('Tool: Fill');await click(30,94);await wait('Filled area');
  await key('F3');await wait('Replace existing image?');await type('y');await wait('Saved BMP');
  bmp=await guest('DRAW.BMP');assert.equal(pixel(bmp,7,7),21);assert.ok(near(bmp,5,5,196),'Rectangle boundary remains after filling');assert.equal(pixel(bmp,0,0),7);
  await key('F2');await wait('Open BMP:');await key('CTRL+U');await type('BAD.BMP\r');await wait('Cannot open BMP:');
  assert.deepEqual(await guest('DRAW.BMP'),bmp,'Malformed input does not alter the saved image');
  if(screenshot) {await pause(150);await page.screenshot({path:screenshot});}
  await key('F10');await wait('KPAINT exited.');
  await type('COPY DRAW.BMP KEEP.BMP\r');await type('ECHO RESERVED>KDIMG.TMP\r');
  await type('KPAINT DRAW.BMP\r');await wait('Opened BMP');await key('SPACE');await key('F3');await wait('Save failed; image retained');
  assert.deepEqual(await guest('DRAW.BMP'),bmp,'A failed image save preserves the original');
  await key('F4');await wait('Save BMP as:');await key('CTRL+U');await type('KEEP.BMP\r');await wait('Replace existing image?');await type('n');
  assert.deepEqual(await guest('KEEP.BMP'),bmp,'Overwrite cancellation preserves the destination');
  await key('F10');await wait('Discard unsaved image changes?');await type('n');
  await wait('KPAINT - Image editor');await key('F10');await wait('Discard unsaved image changes?');await type('y');await wait('KPAINT exited.');
  await type('DEL KDIMG.TMP\r');
  return ['native text clipboard copy/cut/paste and exact CRLF save',
    'live pixel-resolution window movement while held; PEGC palette displays over 200 simultaneous RGB colors',
    'KVIEW 1/4/8/24-bit BMP display/copy, row padding and top-down orientation; KPAINT pencil, undo/redo, selection clipboard, rectangle, flood fill and 8-bit BMP save',
    'malformed BMP rejection, safe image save failure, overwrite cancellation and unsaved-change protection'];
}
