import fs from 'node:fs';
import path from 'node:path';
import YAML from 'yaml';
import {describe,it,expect} from 'vitest';
import {decimal64,validBackendSnapshot} from '../../../conformance/reference/cast-webcast-backend.mjs';
const root=path.resolve('../..');
const source=YAML.parse(fs.readFileSync(path.join(root,'contract/registry/domains/cast/domain.yaml'),'utf8'));
const node=(state='ready',connected=true,peers=0)=>({state,transportConnected:connected,retainedPeerCount:peers,connectedPeerCount:peers});
const sample=()=>({backendContext:{controlVersion:1,runtimeInstanceId:'11111111-1111-4111-8111-111111111111',backendRevision:'2',backendEventSequence:'2'},state:{enabled:true,state:'ready',selectedBackend:'local',cloud:node('offline',false,1),local:{fallbackEnabled:true,serviceState:'running',servicePort:78,startAttempts:'1',signaling:node()}}});
describe('optional independent WebCast backend contract',()=>{
 it('binds one additive method/event/capability and the same snapshot',()=>{
  expect(source.methods.find((m:any)=>m.name==='cast.getWebCastBackendState')).toMatchObject({id:0x161A,bitOffset:25,request_schema:'CastGetWebCastBackendStateParams',response_schema:'CastWebCastBackendSnapshot'});
  expect(source.events.find((m:any)=>m.name==='cast.webcastBackendChanged')).toMatchObject({id:0x1613,bitOffset:18,event_schema:'CastWebCastBackendSnapshot'});
  expect(source.capabilities.find((m:any)=>m.name==='cast.webcastBackend')).toMatchObject({id:0x160C,schema:'CastWebCastBackendCapability'});
  expect(source.types.CastStatus.fields.find((f:any)=>f.name==='webcastBackend')).toMatchObject({id:16,required:false});
  expect(source.types.CastControlCapabilities.fields.find((f:any)=>f.name==='webcastBackend')).toMatchObject({id:11,required:false});
  expect(source.types.CastWebCastBackendContext.fields.map((f:any)=>f.name)).toEqual(['controlVersion','runtimeInstanceId','backendRevision','backendEventSequence']);
 });
 it('retains historical facts except the explicitly replaced unpublished control fields',()=>{
  const base=JSON.parse(fs.readFileSync(path.join(root,'conformance/fixtures/cast-control/webcast-backend-baseline.json'),'utf8'));
  for(const group of ['methods','events','capabilities'])for(const old of base[group])expect(source[group].find((x:any)=>x.name===old.name)).toEqual(old);
  const replaced=new Set(['CastStopSessionParams.sessionId','CastPresentationCapability.layoutModes','CastPresentationState.layoutMode','CastControlOperation.kind','CastPresentationSettingsPatch.layoutMode','CastPresentationSettingsValues.layoutMode','CastPresentationStartupSettings.layoutMode','CastPresentationSettingsCapability.fields','CastPresentationSettingsCapability.layoutModes']);
  for(const [name,type] of Object.entries(base.types) as [string,any][])for(const old of type.fields)if(!replaced.has(`${name}.${old.name}`))expect(source.types[name].fields.find((f:any)=>f.name===old.name)).toEqual(old);
 });
 it('allows old P2P peers while cloud is offline and preserves both authorities on recovery',()=>{
  const s=sample();expect(validBackendSnapshot(s)).toBe(true);s.state.cloud=node('ready',true,1);s.state.selectedBackend='cloud';s.state.local.signaling=node('ready',true,2);expect(validBackendSnapshot(s)).toBe(true);
 });
 it('rejects socket-open without room readiness and impossible local port/peer facts',()=>{
  const s=sample();s.state.local.signaling.state='creatingRoom';expect(validBackendSnapshot(s)).toBe(false);
  s.state.local.signaling.state='ready';s.state.local.servicePort=65536;expect(validBackendSnapshot(s)).toBe(false);
  s.state.local.servicePort=78;s.state.cloud.connectedPeerCount=2;expect(validBackendSnapshot(s)).toBe(false);
 });
 it('keeps decimal uint64 values exact and rejects overflow/coercion',()=>{
  for(const v of ['0','9007199254740993','18446744073709551615'])expect(decimal64(v)).toBe(true);
  for(const v of [0,'01','-1','1.0','18446744073709551616',' 1'])expect(decimal64(v)).toBe(false);
  const s=sample();s.backendContext.backendRevision='18446744073709551615';s.state.local.startAttempts='9007199254740993';expect(validBackendSnapshot(s)).toBe(true);
  s.backendContext.controlVersion=2;expect(validBackendSnapshot(s)).toBe(false);
 });
 it('allows unknown optional fields without changing known semantics',()=>{
  const s:any=sample();s.future={kind:'optional'};s.state.cloud.future=true;expect(validBackendSnapshot(s)).toBe(true);
 });
});
