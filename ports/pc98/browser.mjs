// Use CDP over inherited pipes: no dependency on Chrome printing a websocket
// endpoint before Puppeteer's startup deadline. Keep a bounded launch timeout
// and retain browser stderr in CI so startup failures remain diagnosable.
import {createRequire} from 'node:module';
import {join,resolve} from 'node:path';
export function launchBrowser(checkout,options={}){
 const require=createRequire(join(resolve(checkout),'package.json'));
 return require('puppeteer-core').launch({
  executablePath:process.env.CHROMIUM??'/usr/bin/chromium',
  headless:true,pipe:true,timeout:120000,dumpio:process.env.CI==='true',
  args:process.env.WEBNP2_NO_SANDBOX==='1'?['--no-sandbox']:[],
  ...options,
 });
}
