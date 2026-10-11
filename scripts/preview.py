"""Local static preview with the same headers Cloudflare Pages will apply."""
from pathlib import Path
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
import fnmatch

PUBLIC=Path(__file__).resolve().parent.parent / 'public'
rules=[]
for line in (PUBLIC / '_headers').read_text().splitlines():
    if not line.strip(): continue
    if not line.startswith(' '): rules.append((line,[]))
    else:
        name,value=line.strip().split(':',1);rules[-1][1].append((name,value.strip()))
class Handler(SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(PUBLIC),**kwargs)
    def end_headers(self):
        for pattern,headers in rules:
            if fnmatch.fnmatch(self.path.split('?')[0],pattern):
                for name,value in headers:self.send_header(name,value)
        super().end_headers()
    def log_message(self,*args):pass
print('AH-RAY Studio static preview: http://127.0.0.1:8978/',flush=True)
ThreadingHTTPServer(('127.0.0.1',8978),Handler).serve_forever()
