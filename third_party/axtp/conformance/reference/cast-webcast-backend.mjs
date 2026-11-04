// Contract oracle only: no networking, fallback or media policy implementation.
export const decimal64=value=>typeof value==='string'&&/^(0|[1-9][0-9]{0,19})$/.test(value)&&BigInt(value)<=18446744073709551615n;
const u32=v=>Number.isInteger(v)&&v>=0&&v<=4294967295;
const uuid=v=>typeof v==='string'&&/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/.test(v);
function node(n){return n&&['disabled','offline','creatingRoom','awaitingRestore','ready','failed','stopped'].includes(n.state)&&
 typeof n.transportConnected==='boolean'&&u32(n.retainedPeerCount)&&u32(n.connectedPeerCount)&&n.connectedPeerCount<=n.retainedPeerCount&&
 (!('lastNetworkError' in n)||u32(n.lastNetworkError))&&(n.state!=='ready'||n.transportConnected);}
export function validBackendSnapshot(snapshot){
 try{
  const c=snapshot.backendContext,s=snapshot.state,l=s.local;
  if(c.controlVersion!==1||!uuid(c.runtimeInstanceId)||!decimal64(c.backendRevision)||!decimal64(c.backendEventSequence))return false;
  if(typeof s.enabled!=='boolean'||!['disabled','starting','ready','failed','stopped'].includes(s.state)||!['none','cloud','local'].includes(s.selectedBackend))return false;
  if(!node(s.cloud)||!node(l.signaling)||typeof l.fallbackEnabled!=='boolean'||!decimal64(l.startAttempts)||!['disabled','dormant','running','failed','stopped'].includes(l.serviceState))return false;
  if(('lastStartError' in l)&&!u32(l.lastStartError))return false;
  if(l.serviceState==='running'?(!u32(l.servicePort)||l.servicePort<1||l.servicePort>65535||!l.fallbackEnabled):('servicePort' in l))return false;
  if((s.state==='ready')!==(s.selectedBackend!=='none'))return false;
  if(s.selectedBackend==='cloud'&&s.cloud.state!=='ready')return false;
  if(s.selectedBackend==='local'&&(l.serviceState!=='running'||l.signaling.state!=='ready'))return false;
  if(!s.enabled&&(s.state!=='disabled'||s.selectedBackend!=='none'||s.cloud.state!=='disabled'||l.serviceState!=='disabled'||l.signaling.state!=='disabled'||s.cloud.retainedPeerCount||l.signaling.retainedPeerCount))return false;
  if(s.enabled&&s.state==='disabled')return false;
  if(s.state==='stopped'&&(s.selectedBackend!=='none'||s.cloud.transportConnected||l.signaling.transportConnected||l.serviceState==='running'))return false;
  return true;
 }catch{return false;}
}
