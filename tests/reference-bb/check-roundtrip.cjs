// SPDX-License-Identifier: GPL-3.0-or-later
const fs = require('fs');
const {chromium} = require('playwright');
(async () => {
  const source=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
  const browser=await chromium.launch({channel:'chrome',headless:true});
  const page=await browser.newPage();
  const errors=[];page.on('pageerror',e=>errors.push(String(e)));
  await page.goto(process.env.BLOCKBENCH_URL||'http://127.0.0.1:29321',{waitUntil:'networkidle'});
  const result=await page.evaluate(async source=>{
    setupProject(Formats.free);Codecs.project.parse(source,'reyond-roundtrip.bbmodel');
    await new Promise(r=>setTimeout(r,500));Canvas.updateAll();
    return {elements:Outliner.elements.length,groups:Group.all.length,textures:Texture.all.map(t=>[t.width,t.height]),animations:Animation.all.length,output:Codecs.project.compile({raw:true,bitmaps:true})};
  },source);
  if(result.elements!==source.elements.length||result.groups!==source.groups.length||result.animations!==source.animations.length||errors.length)throw Error(JSON.stringify({result,errors}));
  console.log('PASS saved ReyondBroom project opens in the actual reference codec:',JSON.stringify({elements:result.elements,groups:result.groups,textures:result.textures,animations:result.animations}));
  await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
