import fs from "node:fs";
import path from "node:path";
import YAML from "yaml";
import {describe,it,expect} from "vitest";
import {validateValue} from "../../../conformance/reference/cast-control.mjs";
const root=path.resolve("../..");
const domain=YAML.parse(fs.readFileSync(path.join(root,"contract/registry/domains/cast/domain.yaml"),"utf8"));
const field=(schema:string,key:string)=>domain.types[schema].fields.find((f:any)=>f.name===key);
describe("adopted unpublished control refresh",()=>{
 it("separates Layout from content adaptation and removes the obsolete entry gate",()=>{
  for(const s of ["CastPresentationSettingsPatch","CastPresentationSettingsValues","CastPresentationStartupSettings"]){
   expect(field(s,"layoutMode").enum).toEqual(["grid","presentation"]);
   expect(field(s,"contentAdaptation").enum).toEqual(["fixed","auto-fit"]);
   expect(field(s,"presentationEnabled")).toBeUndefined();
   expect(field(s,"presentationSplitOnDoubleClick")).toBeUndefined();
   expect(field(s,"presentationGridOnDoubleClick").type).toBe("bool");
   expect(field(s,"presentationAutoEnter")).toMatchObject({id:13,type:"bool",required:false});
   validateValue(domain.types,s,{presentationAutoEnter:true});
   expect(()=>validateValue(domain.types,s,{presentationAutoEnter:"true"})).toThrow();
  }
  expect(field("CastSetPresentationFocusParams","display").enum).toEqual(["sidebar","grid","focus"]);
  expect(field("CastMeetingPresentationState","autoEnter")).toMatchObject({id:15,type:"bool",required:false});
  expect(field("CastMeetingPresentationState","autoEnterSuppressed")).toMatchObject({id:16,type:"bool",required:false});
  expect(field("CastMeetingPresentationState","view").enum).toEqual(["sidebar","focus"]);
  expect(()=>validateValue(domain.types,"CastPresentationSettingsPatch",{layoutMode:"fixed-ratio"})).toThrow();
  validateValue(domain.types,"CastPresentationSettingsPatch",{layoutMode:"grid"});
  validateValue(domain.types,"CastPresentationSettingsPatch",{contentAdaptation:"auto-fit"});
 });
 it("adds master output state without redefining per-source selection",()=>{
  expect(field("CastAudioPolicyState","masterMuted")).toMatchObject({id:6,type:"bool",required:true});
  expect(field("CastAudioPolicyState","enabledSessions").type).toBe("array");
  expect(field("CastAudioPolicyCapability","supportsMasterMute").type).toBe("bool");
 });
 it("gives secret-free AirPlay state a separate capability and a P0 commit part",()=>{
  expect(domain.events.find((e:any)=>e.name==="cast.airplayChanged")).toMatchObject({id:0x1614,bitOffset:19,event_schema:"CastAirPlayChangedEvent"});
  expect(domain.capabilities.find((c:any)=>c.name==="cast.airplayControl")).toMatchObject({id:0x160E});
  expect(field("CastAirPlayChangedEvent","context").type).toBe("CastControlEventContext");
  for(const k of ["attemptId","challengeId","displayRequestId"])expect(field("CastAirPlayAuthenticationAttempt",k)).toMatchObject({type:"string",max_length:20});
  expect(domain.types.CastAirPlayControlState.fields.map((f:any)=>f.name)).not.toContain("pinCode");
  expect(field("CastAirPlayControlCapability","passwordPolicy").enum).toEqual(["startup-only"]);
 });
});
