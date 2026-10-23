// Shared guest UI test, using only normal DOS commands and real key events.
import assert from 'node:assert/strict';

export async function verifyFiler(page, { screenshot, basic = false } = {}) {
  const screen = () => page.evaluate(() => window.np2debug.np2.getScreenText().text);
  const wait = async needle => page.waitForFunction(
    needle => window.np2debug?.np2?.getScreenText().text.includes(needle),
    { timeout: 30000 }, needle,
  ).catch(async error => { console.error(await screen()); throw error; });
  const key = async value => page.evaluate(value => window.np2debug.np2.sendKeys(value), value);
  const type = async value => {
    await page.evaluate(value => window.np2debug.np2.typeText(value), value);
    await new Promise(resolve => setTimeout(resolve, 250));
  };
  const guest = async name => Buffer.from(await page.evaluate(
    async name => btoa(String.fromCharCode(...await window.np2debug.np2.diskReadFile('fd1', name))), name,
  ), 'base64');
  const select = async name => {
    const lines = (await screen()).split('\n');
    const right = lines[22]?.startsWith('Active: RIGHT');
    const path = lines[1].slice(right ? 40 : 0, right ? 79 : 39).trim().replace(/^[A-Z]:/i, '').replace(/\\/g, '/');
    const entries = await page.evaluate(async path => (await window.np2debug.np2.diskListFiles('fd1', path)).entries, path);
    entries.sort((a,b) => Number(b.isDir)-Number(a.isDir) || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    const names = entries.map(entry => entry.name);
    if (path !== '/') names.unshift('..');
    const target = names.indexOf(name); assert.ok(target >= 0, name + ' must be on disk');
    const selected = expected => page.waitForFunction(expected => {
      const line = window.np2debug.np2.getScreenText().lines[22] ?? '';
      return line.match(/Selected: (.*)/)?.[1]?.trim() === expected;
    }, { timeout: 30000 }, expected);
    await key('HOME'); await selected(names[0]);
    // Wait for the guest to apply each arrow before sending the next one.
    // A longer directory takes enough time to repaint to overrun a blind loop.
    for (let i = 1; i <= target; i++) { await key('DOWN'); await selected(names[i]); }
  };
  const prompt = async (label, value) => {
    await wait(label); await key('CTRL+U'); await type(value + '\r');
  };
  await type('ECHO FILER-GUEST-DATA>SOURCE.TXT\r');
  await wait('A>');
  // An earlier prompt can still be visible while DOS has only created the
  // empty directory entry. Wait for the redirected write and close to finish.
  await page.waitForFunction(async () => {
    try {
      const bytes = await window.np2debug.np2.diskReadFile('fd1', 'SOURCE.TXT');
      return new TextDecoder().decode(bytes) === 'FILER-GUEST-DATA\r\n';
    } catch { return false; }
  }, { timeout: 30000 });
  assert.equal((await guest('SOURCE.TXT')).toString(), 'FILER-GUEST-DATA\r\n');
  await type('FD98\r');
  await wait('FD Filer 1.0');
  await key('TAB'); await wait('Active: RIGHT');
  await key('TAB'); await wait('Active: LEFT');
  await key('F1'); await wait('Keyboard help'); await key('ESC');

  await select('SOURCE.TXT');
  await key('F3'); await wait('FD Filer - Viewer'); await wait('FILER-GUEST-DATA'); await key('ESC');
  await key('F5'); await prompt('Copy to:', 'COPIED.TXT'); await wait('Copied');
  assert.deepEqual(await guest('COPIED.TXT'), await guest('SOURCE.TXT'));
  await key('F5'); await prompt('Copy to:', 'COPIED.TXT'); await wait('Destination exists');
  assert.deepEqual(await guest('COPIED.TXT'), await guest('SOURCE.TXT'));
  await select('COPIED.TXT');
  await key('F2'); await prompt('Rename to:', 'RENAMED.TXT'); await wait('Renamed');
  await key('F7'); await prompt('Create directory:', 'WORK'); await wait('Directory created');

  // The destination defaults to the other pane's directory.
  await key('TAB'); await wait('Active: RIGHT'); await select('WORK'); await key('ENTER'); await wait('A:\\WORK');
  await key('TAB'); await wait('Active: LEFT'); await select('RENAMED.TXT');
  await key('F6'); await wait('Move to:');
  assert.ok((await screen()).includes('A:\\WORK\\RENAMED.TXT'));
  await key('ENTER'); await wait('Moved');
  assert.deepEqual(await guest('WORK/RENAMED.TXT'), await guest('SOURCE.TXT'));
  await key('TAB'); await wait('Active: RIGHT'); await select('RENAMED.TXT');
  await key('F8'); await wait('Delete selected'); await key('N'); await wait('Cancelled');
  assert.deepEqual(await guest('WORK/RENAMED.TXT'), await guest('SOURCE.TXT'));
  await key('F8'); await wait('Delete selected'); await key('Y'); await wait('Deleted');
  await key('BS');
  await page.waitForFunction(() => window.np2debug.np2.getScreenText().lines[1]?.slice(40).trim() === 'A:\\', { timeout: 30000 });
  await key('TAB'); await wait('Active: LEFT'); await select('WORK');
  await key('F8'); await wait('Delete selected'); await key('Y'); await wait('Deleted');

  await select('P98TEST.COM'); await key('F9');
  await wait('DOS4 EXEC OK'); await wait('Program returned. Press any key');
  await key('SPACE'); await wait('FD Filer 1.0'); await wait('Program returned (exit 0)');
  if (basic) {
    await select('PRIMES.BAS'); await key('F9');
    await wait('Total primes found:  46'); await wait('Program returned. Press any key');
    await key('SPACE'); await wait('FD Filer 1.0');
  }
  if (screenshot) await page.screenshot({ path: screenshot });
  await key('F10'); await wait('FD Filer exited.');
  await type('ECHO FILER-DOS-RETURN-OK\r'); await wait('FILER-DOS-RETURN-OK');
  return [
    'native FD98 two-pane listing, cursor/function keys and help',
    'text viewer; byte-exact copy and existing-destination refusal',
    'rename, directory creation/navigation, cross-pane move and confirmed/cancelled deletion',
    'child COM EXEC/return' + (basic ? ' and bundled BASIC sample launch' : '') + '; F10 return to DOS',
  ];
}
