"""Generate disposable reference frames with the original desktop C++ renderer."""
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(sys.argv[1]).resolve() / 'emulator'))
from lab import Native, default_session, strip

native = Native()
s = default_session()
s['nodes'] = [dict(id=i+1, name=f'Parity {i+1}', outputs=[
    strip(1, 3+i, 0, i%3, count=24),
    strip(9, 7-i, 8, i%3, orientation={0:5,1:1,2:2}[i%3], count=17)]) for i in range(3)]
s['nodes'][0]['outputs'][0]['segments'] = [dict(start=3, **strip(4,5,2,1,span=4,count=9)),dict(start=14, **strip(6,6,0,0,span=7,count=8))]
cases=[]
for effect in range(47):
    for speed in (0,108):
        candidate=json.loads(json.dumps(s));candidate['dmx'][7]=effect;candidate['dmx'][5]=speed
        for spatial in (0,13):
            candidate['dmx'][6]=spatial
            native.configure(candidate);native.dll.lab_reset()
            frames=[]
            for now in (0,17,333,1000,2123):
                frames.append(dict(now=now,frames=native.render(candidate,now,0,0,False)))
            cases.append(dict(session=json.loads(json.dumps(candidate)),samples=frames))
for spatial in range(25):
    candidate=json.loads(json.dumps(s));candidate['dmx'][6]=spatial;candidate['dmx'][7]=45
    native.configure(candidate);native.dll.lab_reset()
    cases.append(dict(session=candidate,samples=[dict(now=750,frames=native.render(candidate,750,0,0,False))]))
output=ROOT / '.build/native-fixtures.json'
output.parent.mkdir(exist_ok=True)
output.write_text(json.dumps(cases),encoding='utf-8')
print(f'Generated {len(cases)} scenarios / {sum(len(c["samples"]) for c in cases)} frames from native FX v{native.dll.lab_version()}')
