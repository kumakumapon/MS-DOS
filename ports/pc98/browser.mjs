// Resolve the adapter from the exact webnp2 checkout used to run the guest.
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

export async function launchBrowser(checkout) {
 const adapter = pathToFileURL(resolve(checkout, 'scripts/legacy-browser.mjs')).href;
 const { launchDosBrowser } = await import(adapter);
 return launchDosBrowser({
  executablePath: process.env.CHROMIUM ?? '/usr/bin/chromium',
  headless: true,
  timeout: 120000,
  args: process.env.WEBNP2_NO_SANDBOX === '1' ? ['--no-sandbox'] : [],
 });
}
