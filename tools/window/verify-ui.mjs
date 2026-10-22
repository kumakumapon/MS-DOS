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
    await page.waitForFunction(() => window.np2debug.np2.getScreenText().lines[24]?.includes('Help'),
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
  await wait('Ready'); await key('v'); await wait('Launcher list view');
  const point = (x, y) => page.evaluate(({ x, y }) => {
    const source = document.querySelector('#canvas'), canvas = document.createElement('canvas');
    canvas.width = source.width; canvas.height = source.height;
    const ctx = canvas.getContext('2d'); ctx.drawImage(source, 0, 0);
    return Array.from(ctx.getImageData(x, y, 1, 1).data).slice(0, 3);
  }, { x, y });
  assert.ok((await point(0,32)).every((c,i) => Math.abs(c-[16,32,49][i])<=4), 'Desktop uses the PEGC gradient palette (RGB565 output tolerance)');
  assert.ok((await point(24,32)).every(c => Math.abs(c-255)<=4), 'Window frame is drawn by the guest');
  assert.ok((await point(35,48)).every((c,i) => Math.abs(c-[51,102,170][i])<=4), 'Active title bar has the focus color');
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
  await key('F3'); await wait('Parent'); await key('v'); await wait('Files list view');
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
  await type('KDESK\r'); await title('Launcher', 3, 5); await key('v'); await wait('Launcher list view');
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
  await cell(49, 5); await wait('Window minimized');
  assert.ok(!(await text()).includes('Launcher'));
  await clickTask(page,'Apps'); await title('Launcher', 5, 11);
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
  await cell(32, 0); await title('Files', 4, 9); await wait('Parent'); await key('v'); await wait('Files list view');
  const names = await page.evaluate(async () =>
    (await window.np2debug.np2.diskListFiles('fd1')).entries.map(entry => entry.name));
  const mouseSelect = async name => {
    const index = names.indexOf(name);
    assert.ok(index >= 0, name + ' must be on disk');
    // Keyboard navigation keeps the selected list row visible; the dedicated
    // explorer suite below tests independent mouse scrolling.
    await key('HOME');
    for (let i = 0; i < index; i++) await key('DOWN');
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
  const explorerResults = await verifyExplorer(page, { screenshot: screenshot?.replace(/\.png$/, '-explorer.png') });
  const mediaResults = await verifyMedia(page, { screenshot: screenshot?.replace(/\.png$/, '-media.png') });
  return [
    ...explorerResults,
    ...mediaResults,
    ...await verifyUtilities(page, { screenshot: screenshot?.replace(/\.png$/, '-utilities.png') }),
    ...await verifyRedraw(page),
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
  const pixel=(bmp,x,y)=>bmp[1078+(bmp.readInt32LE(22)-y-1)*((bmp.readInt32LE(18)+3)&~3)+x];
  // Absolute bus-mouse delivery can differ by two pixels from the host estimate.
  const near=(bmp,x,y,c)=>[-2,-1,0,1,2].some(dy=>[-2,-1,0,1,2].some(dx=>pixel(bmp,x+dx,y+dy)===c));
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
  await key('F2'); await wait('Enter opens'); await page.evaluate(() => window.np2debug.np2.mouseHome()); await pause(500);
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
    return [51,102,170].every((v,i)=>Math.abs(p[i]-v)<=4);
  },{timeout:60000});
  const livePixel = await page.evaluate(() => {
    const source=document.querySelector('#canvas'), c=document.createElement('canvas'); c.width=source.width;c.height=source.height;
    const g=c.getContext('2d');g.drawImage(source,0,0);return Array.from(g.getImageData(84,90,1,1).data).slice(0,3);
  });
  assert.ok(livePixel.every((c,i) => Math.abs(c-[51,102,170][i])<=4),'Window contents move at pixel resolution while the button remains held: '+livePixel);
  await page.evaluate(async () => { window.mediaRelease(); await window.mediaDrag; window.np2debug.np2.mouseMoveTo=window.mediaOriginalMove; });
  // Foreground image apps inherit and return the desktop's bus-mouse position.
  // Do not home/resync between the parent and child: that would conceal a reset.
  await key('F9');await wait('KPAINT - Image editor');await wait('Ready');
  await click(116,140);await wait('Image changed');await key('F4');await wait('Save BMP as:');
  await key('CTRL+U');await type('BRIDGE.BMP\r');await wait('Saved BMP');
  assert.ok(near(await guest('BRIDGE.BMP'),50,30,196),'KDESK passes its current mouse position to KPAINT');
  await key('F10');await wait('Press any key or click to return to KDESK');await click(600,350);await wait('Program returned to desktop');
  await pause(200);
  const returnedPointer=await page.evaluate(()=>{
    const source=document.querySelector('#canvas'),c=document.createElement('canvas');c.width=source.width;c.height=source.height;
    const g=c.getContext('2d');g.drawImage(source,0,0);const p=g.getImageData(598,348,6,6).data;
    for(let i=0;i<p.length;i+=4) if([34,34,51].every((v,j)=>Math.abs(p[i+j]-v)<=4)) return true;
    return false;
  });
  assert.ok(returnedPointer,'KPAINT returns its updated mouse position to KDESK');
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
  return ['native text clipboard copy/cut/paste and exact CRLF save; parent/child mouse continuity without resync',
    'live pixel-resolution window movement while held; PEGC palette displays over 200 simultaneous RGB colors',
    'KVIEW 1/4/8/24-bit BMP display/copy, row padding and top-down orientation; KPAINT pencil, undo/redo, selection clipboard, rectangle, flood fill and 8-bit BMP save',
    'malformed BMP rejection, safe image save failure, overwrite cancellation and unsaved-change protection'];
}

export async function verifyUtilities(page, { screenshot } = {}) {
  const pause = ms => new Promise(resolve => setTimeout(resolve,ms));
  const text = () => page.evaluate(()=>window.np2debug.np2.getScreenText().text);
  const wait = needle => page.waitForFunction(needle=>window.np2debug.np2.getScreenText().text.includes(needle),{timeout:60000},needle)
    .catch(async error=>{console.error(await text());throw error;});
  const key = async value => {await page.evaluate(value=>window.np2debug.np2.sendKeys(value),value);await pause(150);};
  const type = async value => {await page.evaluate(value=>window.np2debug.np2.typeText(value),value);await pause(150);};
  const click = async(x,y,count=1)=>{await page.evaluate(({x,y})=>window.np2debug.np2.mouseMoveTo(x,y),{x,y});await pause(200);await page.evaluate(count=>window.np2debug.np2.mouseClick({count}),count);await pause(200);};
  const selected = name => wait('Selected: '+name);
  const value = expected => wait('Value: '+expected);
  const clipboard = ()=>page.evaluate(()=>{
    const s=atob(window.np2debug.np2.readMemoryBase64(0x1000,0x9f000).base64);let at=-1;
    while((at=s.indexOf('KDESKCLP',at+1))>=0) {
      const word=o=>s.charCodeAt(at+o)|(s.charCodeAt(at+o+1)<<8);
      if(word(8)===1&&word(10)===1) return s.slice(at+30,at+30+word(12));
    } return null;
  });
  const time = ()=>page.evaluate(()=>window.np2debug.np2.getScreenText().text.match(/\b\d{2}:\d{2}:\d{2}(?: [AP]M)?\b/)?.[0]);
  const clockReady = ()=>page.waitForFunction(()=>/\b\d{2}:\d{2}:\d{2}\b/.test(window.np2debug.np2.getScreenText().text),{timeout:60000});
  await type('KDESK\r');await wait('Ready');await selected('File manager');await wait('List');
  await page.evaluate(()=>window.np2debug.np2.mouseHome());await pause(500);
  await key('DOWN');await selected('BASIC');await key('DOWN');await selected('Image editor');await key('RIGHT');await selected('Calculator');
  // The last row scrolls into view. Single click selects; double click opens.
  await click(208,160);await selected('Calculator');assert.ok(!(await text()).includes('Value:'),'One icon click selects without launching');
  await click(208,160,2);await value('0');
  await type('12.5*4=');await value('50');await key('ENTER');await value('200');
  await type('c0.1+0.2=');await value('0.3');await key('CTRL+C');await wait('Copied calculator result');assert.equal(await clipboard(),'0.3');
  // Calculator and Memo share the existing desktop clipboard.
  await key('F4');await wait('NOTE.TXT');await key('CTRL+A');await key('CTRL+V');await wait('Pasted clipboard');
  assert.ok((await text()).split('\n').some(line=>line.trim()==='0.3_'));
  await key('ESC');await wait('Unsaved memo:');await type('d');await wait('Memo closed');
  await value('0.3'); // Closing Memo already focuses Calculator.
  await type('c9/0=');await value('Divide by zero');await type('c99999999*99999999=');await value('Overflow');
  await type('c7*6=');await value('42');
  // Real mouse keys, including the memory row and Copy/Paste footer.
  await click(284,136);await wait('Calculator ready'); // M+
  await type('c');await value('0');await click(156,136);await value('42'); // MR
  await click(100,328);await wait('Copied calculator result');assert.equal(await clipboard(),'42');
  await type('c');await click(180,328);await value('42');await wait('Pasted calculator number');
  await key('F7');for(let i=0;i<6;i++)await key('LEFT');for(let i=0;i<4;i++)await key('UP');await key('ENTER');await value('42');await wait('Copy');
  await key('F2');await key('RIGHT');await selected('Clock');await key('ENTER');await clockReady();await wait('12-hour');
  await key('F2');await key('ESC');await value('42'); // Close launcher; calculator has focus.
  const before=await time();await page.waitForFunction(before=>{
    const match=window.np2debug.np2.getScreenText().text.match(/\b\d{2}:\d{2}:\d{2}\b/);return match&&match[0]!==before;
  },{timeout:10000},before);
  await type('c6*7=');await value('42');await clockReady();
  assert.ok((await text()).includes(String(new Date().getFullYear())+'-'),'Clock reads the DOS RTC date');
  if(screenshot){await pause(300);await page.screenshot({path:screenshot});}
  await clickTask(page,'Clock');await key('t');await wait('Clock 12-hour display');
  await page.waitForFunction(()=>/\b\d{2}:\d{2}:\d{2} [AP]M\b/.test(window.np2debug.np2.getScreenText().text));
  await key('CTRL+C');await wait('Copied clock date/time');assert.match(await clipboard(),/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} [AP]M$/);
  await key('t');await wait('Clock 24-hour display');assert.ok(!/\b\d{2}:\d{2}:\d{2} [AP]M\b/.test(await text()));
  await key('F8');await wait('Window minimized');assert.equal(await time(),undefined);
  await clickTask(page,'Clock');await clockReady();await key('ESC');
  await key('F2');await key('v');await wait('Launcher list view');await key('v');await wait('Launcher icon view');
  await key('F7');for(let i=0;i<20;i++)await key('LEFT');for(let i=0;i<10;i++)await key('UP');await key('ENTER');
  await key('HOME');await selected('File manager');for(let i=0;i<8;i++)await key('DOWN');await selected('Clock');await key('ENTER');await clockReady();
  await key('F10');await wait('KDESK exited.');
  return ['native calculator decimal/chain/repeated arithmetic, errors, memory and mouse keypad',
    'calculator/Memo clipboard and safe resize constraints; live clock while calculator is active',
    'DOS RTC date, 12/24-hour display, copy, minimize/restore and close',
    'default icon launcher, grid/list toggle, keyboard navigation, mouse select/double-click and narrow-window scrolling'];
}

// Observe displayed frames, not just the final screen: a full clear can finish
// correctly and still flash the background for several video frames per key.
export async function verifyRedraw(page) {
  const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  const key=async key=>{await page.evaluate(key=>window.np2debug.np2.sendKeys(key),key);await pause(180);};
  const wait=needle=>page.waitForFunction(needle=>window.np2debug.np2.getScreenText().text.includes(needle),{timeout:60000},needle);
  await page.evaluate(()=>window.np2debug.np2.typeText('KDESK\r'));await wait('Ready');
  await page.waitForFunction(()=>window.np2debug.np2.getScreenText().lines[24]?.includes('Apps'));await pause(150);
  const edge=()=>page.evaluate(()=>{
    const c=document.createElement('canvas');c.width=640;c.height=400;const g=c.getContext('2d');
    g.drawImage(document.querySelector('#canvas'),0,0);return Array.from(g.getImageData(472,200,24,1).data);
  });
  await page.evaluate(()=>window.np2debug.np2.mouseHome());await pause(500);
  for(const [from,to,description] of [
    [{x:403,y:285},{x:483,y:333},'resize preview'],
    [{x:100,y:52},{x:300,y:120},'live window move'],
  ]) {
    const clean=await edge();
    await page.evaluate(({from,to})=>{
      const np2=window.np2debug.np2,original=np2.mouseMoveTo.bind(np2);window.resizeMove=original;window.resizeRelease=null;
      np2.mouseMoveTo=async(x,y)=>{const value=await original(x,y);if(x===to.x&&y===to.y)await new Promise(resolve=>window.resizeRelease=resolve);return value;};
      window.resizeHeld=np2.mouseDrag(from,to);
    },{from,to});
    await page.waitForFunction(()=>!!window.resizeRelease);
    await page.waitForFunction(clean=>{
      const c=document.createElement('canvas');c.width=640;c.height=400;const g=c.getContext('2d');
      g.drawImage(document.querySelector('#canvas'),0,0);return Array.from(g.getImageData(472,200,24,1).data).some((v,i)=>v!==clean[i]);
    },{timeout:60000},clean);
    await key('ESC');
    await page.evaluate(async()=>{window.resizeRelease();await window.resizeHeld;window.np2debug.np2.mouseMoveTo=window.resizeMove;});
    await pause(250);assert.deepEqual(await edge(),clean,`Closing during a held ${description} restores pixels beyond the old window`);
    await key('F2');await wait('Selected: File manager');
  }
  await page.evaluate(()=>window.np2debug.np2.mouseMoveTo(600,350));await pause(300);
  await page.evaluate(()=>{
    const source=document.querySelector('#canvas'),c=document.createElement('canvas');c.width=640;c.height=400;
    const g=c.getContext('2d',{willReadFrequently:true});
    window.redrawProbe={frames:0,flashes:0,run:true};
    const frame=()=>{
      const probe=window.redrawProbe;if(!probe.run)return;
      g.drawImage(source,0,0);const p=g.getImageData(1,1,1,1).data;
      probe.frames++;if(![34,34,51].every((v,i)=>Math.abs(v-p[i])<=4))probe.flashes++;
      requestAnimationFrame(frame);
    };requestAnimationFrame(frame);
  });
  try {
    for(let i=0;i<4;i++)for(const direction of ['DOWN','RIGHT','UP','LEFT'])await key(direction);
    await key('v');await wait('Launcher list view');
    for(let i=0;i<4;i++){await key('DOWN');await key('UP');}
    for(const window of ['F4','F2','F1','F8','TAB','ESC','F4','F2'])await key(window);
    // A background clock partially covered by Memo used to force full clears.
    await key('v');await key('HOME');await key('HELP');await wait('Selected: Clock');await key('ENTER');
    await wait('12-hour');await key('F4');await wait('NOTE.TXT');await pause(2500);
  } finally {
    await page.evaluate(()=>{window.redrawProbe.run=false;});
  }
  const probe=await page.evaluate(()=>window.redrawProbe);
  assert.ok(probe.frames>20,'Sample enough video frames during real guest operations');
  assert.equal(probe.flashes,0,`The menu stays intact through selections, focus and clock updates (${probe.frames} video frames)`);
  await key('F10');await wait('KDESK exited.');
  return [`displayed-frame regression: no desktop/menu clear during icon/list selection, window switching or occluded clock updates (${probe.frames} frames)`];
}

export async function verifyExplorer(page, { screenshot } = {}) {
  const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  const text=()=>page.evaluate(()=>window.np2debug.np2.getScreenText().text);
  const wait=needle=>page.waitForFunction(needle=>window.np2debug.np2.getScreenText().text.includes(needle),{timeout:60000},needle)
    .catch(async error=>{console.error(await text());throw error;});
  const key=async value=>{await page.evaluate(v=>window.np2debug.np2.sendKeys(v),value);await pause(180);};
  const type=async value=>{await page.evaluate(v=>window.np2debug.np2.typeText(v),value);await pause(180);};
  const click=async(x,y,count=1)=>{await page.evaluate(({x,y})=>window.np2debug.np2.mouseMoveTo(x,y),{x,y});await pause(160);await page.evaluate(count=>window.np2debug.np2.mouseClick({count}),count);await pause(200);};
  const drag=async(from,to)=>{await page.evaluate(({from,to})=>window.np2debug.np2.mouseDrag(from,to),{from,to});await pause(250);};
  const read=name=>page.evaluate(async name=>Array.from(await window.np2debug.np2.diskReadFile('fd1',name)),name);
  const original=await read('NOTE.TXT');
  // A directory longer than both viewports tests scrolling independently of
  // selection, including opening an item after the thumb has moved.
  await type('MD SCROLL\r');
  for(let i=0;i<28;i++)await page.evaluate(async i=>window.np2debug.np2.diskWriteFile('fd1',`SCROLL/F${String(i).padStart(2,'0')}.TXT`,new TextEncoder().encode(`EXPLORER FILE ${i}\r\n`)),i);
  await type('KDESK\r');await wait('Ready');await key('ENTER');await wait('Parent');await wait('FD98');
  await key('HELP');await key('ENTER');await wait('A:\\SCROLL');await wait('F00.TXT');
  await page.evaluate(()=>window.np2debug.np2.mouseHome());await pause(500);
  // File icons at (9,8); three columns, two rows. A click selects without EXEC.
  await click(112,144);assert.ok(!(await text()).includes('EDIT98'),'Single file-icon click only selects');
  await click(420,264);await wait('F03.TXT');assert.ok(!(await text()).includes('F00.TXT'),'Down arrow scrolls one icon row');
  await click(420,136);await wait('F00.TXT');
  await click(420,220);await wait('F06.TXT');assert.ok(!(await text()).includes('F00.TXT'),'Track click pages the viewport');
  await key('HOME');await wait('F00.TXT');
  await drag({x:420,y:148},{x:420,y:251});await wait('F27.TXT');
  await key('F4');await wait('NOTE.TXT');await key('F3');await wait('F27.TXT');
  assert.ok(!(await text()).includes('F00.TXT'),'Files retains its own scroll position across focus changes');
  await key('HOME');await key('ROLLDOWN');await wait('F06.TXT');await key('ROLLUP');await wait('F00.TXT');
  await click(112,144,2);await wait('EDIT98');await wait('EXPLORER FILE 0');
  await key('F10');await wait('Press any key or click to return to KDESK');await key('SPACE');await wait('Program returned to desktop');
  await click(420,264);await wait('F03.TXT');await click(112,144,2);await wait('EDIT98');await wait('EXPLORER FILE 3');
  await key('F10');await wait('Press any key or click to return to KDESK');await key('SPACE');await wait('Program returned to desktop');
  await key('v');await wait('Files list view');await key('HOME');await wait('F00.TXT');
  await drag({x:420,y:152},{x:420,y:251});await wait('F27.TXT');
  // Reflow at the minimum size must retain access to the final file and footer.
  await key('v');await key('F7');for(let i=0;i<18;i++)await key('LEFT');for(let i=0;i<6;i++)await key('UP');await key('ENTER');
  await key('HELP');await wait('F27.TXT');await wait('Parent');await key('HOME');await wait('F00.TXT');
  if(screenshot){await page.waitForFunction(()=>window.np2debug.np2.getScreenText().lines[11]?.slice(29,33)==='List');await pause(200);await page.screenshot({path:screenshot.replace(/\.png$/,'-narrow.png')});}
  await key('BS');await wait('A:\\');
  // FD98 remains available for copy/move/delete operations.
  await key('f');await wait('FD Filer 1.0');await key('F10');await wait('Press any key or click to return to KDESK');await key('SPACE');await wait('Program returned to desktop');
  await key('F4');await key('CTRL+A');
  const note=Array.from({length:25},(_,i)=>`ROW${String(i).padStart(2,'0')}`).join('\r');
  await type(note);await wait('ROW24');await key('HOME');await wait('ROW00');
  assert.ok(!(await text()).includes('ROW24'),'Memo Home displays the first page without changing text');
  await click(452,264);await wait('ROW10');assert.ok(!(await text()).includes('ROW00'),'Memo arrow scrolls without moving the append position');
  await key('HOME');await drag({x:452,y:136},{x:452,y:259});await wait('ROW24');
  await key('ROLLUP');assert.ok(!(await text()).includes('ROW24'));await key('HELP');await wait('ROW24');
  await type('X');await wait('ROW24X');assert.deepEqual(await read('NOTE.TXT'),original,'Scrolling and editing do not implicitly save');
  await key('ESC');await wait('Unsaved memo:');await type('d');await wait('Memo closed');assert.deepEqual(await read('NOTE.TXT'),original);
  await key('F1');await wait('F6 then arrows: move window');await key('HELP');await wait('Files F: open FD98 file tools');
  await key('F7');for(let i=0;i<18;i++)await key('LEFT');for(let i=0;i<6;i++)await key('UP');await key('ENTER');await key('HOME');await wait('F1 Help');await key('HELP');await wait('tools');
  await key('F5');await key('F7');for(let i=0;i<18;i++)await key('LEFT');for(let i=0;i<6;i++)await key('UP');await key('ENTER');await key('HELP');await wait('MIT license');
  await key('F2');await key('HOME');await wait('Selected: File manager');await click(388,232);await wait('Calculator');
  assert.ok(!(await page.evaluate(()=>window.np2debug.np2.getScreenText().lines[7])).includes('File manager'),'Launcher also supports independent scrolling');
  // Capture a normal-sized explorer, using fresh default window geometry.
  await key('F10');await wait('KDESK exited.');await type('KDESK\r');await wait('Ready');await key('ENTER');await wait('Parent');await key('HELP');await key('ENTER');await wait('A:\\SCROLL');await wait('F00.TXT');
  if(screenshot){await page.waitForFunction(()=>window.np2debug.np2.getScreenText().lines[17]?.slice(29,33)==='List');await pause(200);await page.screenshot({path:screenshot});}
  await key('F10');await wait('KDESK exited.');
  return ['independent Launcher/Files/Memo/Help/About scroll positions, arrow and track paging, proportional thumb dragging, keyboard paging and resize clamping',
    'default icon file manager from Launcher, file-type icons, selection, directory/parent navigation, TXT double-click after mouse scrolling, list toggle and FD98 access',
    'memo scrollback, append follows the last line, and discard preserves the original disk file'];
}

// Resolve task positions from the guest's visible labels: slots compact as
// windows open/close, so callers must not depend on a historical fixed column.
export async function clickTask(page,name) {
  let col=-1;
  for(let attempt=0;attempt<80;attempt++){
    await new Promise(resolve=>setTimeout(resolve,150));
    col=await page.evaluate(name=>window.np2debug.np2.getScreenText().lines[24]?.indexOf(name)??-1,name);
    if(col>=0)break;
  }
  assert.ok(col>=0,`Taskbar must contain ${name}`);
  await page.evaluate(x=>window.np2debug.np2.mouseMoveTo(x,392),(col+name.length/2)*8);
  await new Promise(resolve=>setTimeout(resolve,200));
  await page.evaluate(()=>window.np2debug.np2.mouseClick());
  await new Promise(resolve=>setTimeout(resolve,250));
}
