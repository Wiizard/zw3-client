import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
const root=process.argv[2]||process.env.ZW3_MENU_ROOT||process.env.ZW3_GAME_ROOT;
if(!root)throw Error('Supply the private game/menu root.');
for(const name of ['zwnet_matchmaking.menu','zwnet_party_lobby.menu']){
  const source=fs.readFileSync(path.join(root,'zw3/core/ui_mp',name),'utf8');
  const fallback=fs.readFileSync(path.join(root,'userraw/ui_mp',name),'utf8');
  assert.equal(source,fallback,`${name}: fallback menu differs`);
  // Validate strings, comments and delimiters, including the local macro bodies.
  // This is static validation, not the game's renderer or packed include parser.
  const stack=[];let quote=false,lineComment=false,blockComment=false;
  for(let i=0;i<source.length;i++){
    const c=source[i],next=source[i+1];
    if(lineComment){if(c==='\n')lineComment=false;continue;}
    if(blockComment){if(c==='*'&&next==='/'){blockComment=false;i++;}continue;}
    if(quote){if(c==='\\'){i++;continue;}if(c==='"')quote=false;continue;}
    if(c==='/'&&next==='/'){lineComment=true;i++;continue;}
    if(c==='/'&&next==='*'){blockComment=true;i++;continue;}
    if(c==='"'){quote=true;continue;}
    if('({['.includes(c))stack.push(c);
    if(')}]'.includes(c))assert.equal(stack.pop(),{')':'(',']':'[','}':'{'}[c],`${name}: delimiter at ${i}`);
  }
  assert.equal(quote,false,`${name}: unterminated string`);assert.equal(blockComment,false);
  assert.equal(stack.length,0,`${name}: unclosed delimiter`);
  assert.doesNotMatch(source,/ZWNET_ToggleReady|zwnet_all_ready|zwnet_ready_pending|WAITING_FOR_READY|READY_CHECK/);
  for(const prefix of ['a','b'])if(name==='zwnet_matchmaking.menu'){
    const line=source.split('\n').find(line=>line.includes(`name "image_map_preview_vote_${prefix}"`));
    const rect=line.match(/rect (\d+) (\d+) (\d+) (\d+)/).slice(1).map(Number);
    assert.ok(rect[0]+rect[2]<94,'thumbnail overlaps map name');
    assert.ok(94+158<261,'map name overlaps vote count');
    assert.ok(rect[2]/rect[3]<2,'thumbnail has a panoramic crop');
    assert.ok(source.includes(`name "zwnet_vote_${prefix}_button"`));
  }
  console.log(`${name}: balanced syntax, matching load paths, no Ready controls, bounded vote geometry`);
}
