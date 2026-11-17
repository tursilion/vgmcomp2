# extractvoice.py - extract MOD voice samples to 16-bit WAVs at the playback rate of each (ins,period) used
# usage: python extractvoice.py <mod> <outdir> <voice ins list>
import sys,struct,os
mod,outdir=sys.argv[1],sys.argv[2]; voices=set(map(int,sys.argv[3].split(',')))
d=open(mod,'rb').read()
order=list(d[952:952+d[950]]); npat=max(order)+1
off=1084+npat*1024; smps={}
for i in range(31):
    o=20+i*30; ln=int.from_bytes(d[o+22:o+24],'big')*2
    smps[i+1]=d[off:off+ln]; off+=ln
pairs=set()
for p in set(order):
    for k in range(256):
        b=d[1084+p*1024+k*4:][:4]
        ins=(b[0]&0xf0)|(b[2]>>4); per=((b[0]&0xf)<<8)|b[1]
        if ins in voices and per: pairs.add((ins,per))
for ins,per in sorted(pairs):
    rate=round(3546895/per)
    pcm=b''.join(struct.pack('<h',(x-256 if x>127 else x)*256) for x in smps[ins])
    fn=os.path.join(outdir,f'v{ins:02d}_{per}.wav')
    with open(fn,'wb') as f:
        f.write(b'RIFF'+struct.pack('<I',36+len(pcm))+b'WAVEfmt '+struct.pack('<IHHIIHH',16,1,1,rate,rate*2,2,16)+b'data'+struct.pack('<I',len(pcm))+pcm)
    print(fn,rate,len(smps[ins])/rate,'s')
