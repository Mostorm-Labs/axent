import {describe,it,expect} from 'vitest';
import {loadProtocolSources} from './sourceLoader.js';
import {buildProtocolDefinition} from './protocolBuilder.js';
import {validateSpec} from './validator.js';
describe('receiver input policy control draft',()=>{
 it('registers additive routes without reviving retired wire modes',async()=>{
  const s=await loadProtocolSources('../..');
  for(const [i,name] of ['getInputMode','setInputMode','getTouchbackMaster','setTouchbackMaster'].entries())
   expect(s.methods.find(m=>m.name===`cast.${name}`)).toMatchObject({id:0x1620+i,bitOffset:31+i,recommendedEncoding:['json']});
  expect(s.events.find(e=>e.name==='cast.touchbackMasterChanged')).toMatchObject({id:0x1616,bitOffset:21});
  expect(s.methods.some(m=>m.id===0x161e)).toBe(false);
  expect(()=>validateSpec(s)).not.toThrow();
  const fields=(name:string)=>s.schemas.find(t=>t.name===name)!.fields;
  expect(fields('CastGestureSettings').find(f=>f.name==='scrollSpeed')).toMatchObject({type:'number',min:0.5,max:4,required:false});
  for(const name of ['CastSetInputModeRequest','CastSetTouchbackMasterRequest'])
   for(const key of ['controlVersion','runtimeInstanceId','commandId'])expect(fields(name).find(f=>f.name===key)?.required).toBe(true);
  expect(buildProtocolDefinition(s).inputStream?.touch.recordBytes).toBe(7);
 });
});
