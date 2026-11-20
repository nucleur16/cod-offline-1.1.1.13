// Actual caller statements with explicit GSC call-frame/end-on boundaries.
// A callee endon returns undefined to an uncancelled caller; root cancellation
// must happen before its original boolean evaluation or movement continuation.
const fs=require('fs'),assert=require('assert'),root=__dirname+'/../bin/home/codol_rawfiles/animscripts/';
function body(source,name){const at=source.indexOf('\n'+name+'('),open=source.indexOf('{',at);assert(at>=0);let end=open+1,depth=1;while(depth){depth+=(source[end]==='{')-(source[end]==='}');end++;}return source.slice(open+1,end-1);}
for(const kind of ['move','combat','stop']){
 const source=fs.readFileSync(root+'soldier_'+kind+'.gsc','utf8'),main=body(source,'main');
 const registration=main.slice(0,main.indexOf(';')+1)+main.slice(main.indexOf(';')+1,main.indexOf('self endon( "death" );')+'self endon( "death" );'.length);
 assert(registration.includes('self endon( "death" );'),kind+' root death owner');
 const call=source.slice(source.indexOf('if ( react_to_grenade() )'),source.indexOf('if ( react_to_grenade() )')+'if ( react_to_grenade() )'.length);
 assert(call.startsWith('if ('),kind);
 for(const oldMissingEndon of [false,true]){
  const events=new Set();let continued=0,casts=0;
  const self={endon:e=>events.add(e),notify(){}};
  const strictBool=value=>{casts++;if(value===undefined)throw Error('cannot cast undefined to bool');return !!value;};
  function* reaction(){yield 'grenade-animation';return undefined;} // callee death endon
  const code=registration.replace(/self endon\(/g,'this.endon(').replace(/self notify\(/g,'this.notify(').replace(oldMissingEndon?'this.endon( "death" );':'__never__','');
  const run=Function('react_to_grenade','strictBool','continued','return function*(){'+code+call.replace('react_to_grenade()', 'strictBool(yield* react_to_grenade())')+'{} continued();}')(reaction,strictBool,()=>continued++).call(self);
  assert.equal(run.next().value,'grenade-animation');
  if(events.has('death'))run.return(); // native death notify kills owning root
  else assert.throws(()=>run.next(),/cannot cast undefined to bool/);
  assert.equal(continued,0,kind+' cannot resume movement against dead/reused self');
  assert.equal(casts,oldMissingEndon?1:0,kind+' cancellation precedes boolean cast');
 }
}
console.log('Actual soldier move/combat/stop root cancellation kills grenade caller before undefined boolean evaluation or dead/reused-entity continuation; prior missing-endon reproduces failure');
