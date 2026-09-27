#!/usr/bin/env python3
"""Run the real C++ client against two isolated HTTP endpoints and a proxy.
No user accounts, local score databases or copyrighted assets are used.
"""
import collections
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

TJA = b'TITLE:Original\nBPM:120\nWAVE:fixture.ogg\nCOURSE:Oni\nLEVEL:8\n#START\n1234,\n#END\nCOURSE:Hard\nLEVEL:4\nSTYLE:Double\n#START P1\n1111,\n#END\n#START P2\n2222,\n#END\n'
AUDIO = b'OggS synthetic download fixture (protocol test, not decoded)' * 8192
SONG, VERSION = '1'*32, 'a'*32
counts = collections.Counter()
stored = {}
lock = threading.Lock()

def score(endpoint, **changes):
    s = dict(id=endpoint+'initial', songId=SONG, versionId=VERSION,
             difficulty='Oni', good=10, ok=2, bad=1,
             score=700000 if endpoint=='second' else 900000, drumroll=5,
             max_combo=6 if endpoint=='second' else 8)
    s.update(changes)
    return s

def chart(endpoint):
    return dict(id=SONG, versionId=VERSION, title='Second' if endpoint=='second' else 'First', subtitle='', maker='A | B',
                titleTranslations={'ja':'日本語タイトル'}, subtitleTranslations={},
                tjaHash=hashlib.sha256(TJA).hexdigest(), audioHash=hashlib.sha256(AUDIO).hexdigest(),
                encoding='utf-8', bpm=120, demoStart=0, audioName='fixture.ogg' if endpoint=='second' else 'Mistletoe.MP3',
                difficulties=[dict(course='Oni', level=8, blockIndex=0, player='', cloudScoreEligible=True),
                              dict(course='Hard', level=4, blockIndex=1, player='P1', cloudScoreEligible=False),
                              dict(course='Hard', level=4, blockIndex=2, player='P2', cloudScoreEligible=False)])

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def reply(self, data, status=200):
        data = data if isinstance(data, bytes) else json.dumps(data).encode()
        self.send_response(status)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)
    def transfer(self, data, known_length=True):
        self.send_response(200)
        if known_length:
            self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        chunk = 16384 if len(data) > 1024 else 32
        try:
            for start in range(0, len(data), chunk):
                self.wfile.write(data[start:start + chunk])
                self.wfile.flush()
                time.sleep(0.01)
        except (BrokenPipeError, ConnectionResetError):
            pass  # A cancelled client intentionally closes mid-transfer.
    def do_GET(self): self.handle_request()
    def do_POST(self): self.handle_request()
    def handle_request(self):
        path=urlsplit(self.path).path
        variant=''
        for prefix in ('missing-combo', 'null-combo', 'refresh', 'guest'):
            if path.startswith('/'+prefix+'/'):
                variant=prefix
                path=path[len(prefix)+1:]
                break
        endpoint='second' if path.startswith('/second/') else 'first'
        if endpoint=='second': path=path[len('/second'):]
        with lock:
            counts[(variant or endpoint)+':'+path]+=1
            if self.path.startswith('http://'): counts['proxy']+=1
        if self.headers.get('Origin') or self.headers.get('Cookie'):
            return self.reply({'code':'UNEXPECTED_BROWSER_AUTH'},400)
        if path=='/api/v1/game/login':
            body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            if body!={'username':'fixture','password':'fixture-password'}: return self.reply({},401)
            return self.reply({'accessToken':('b' if endpoint=='first' else 'c')*64})
        authenticated = self.headers.get('Authorization')=='Bearer '+('b' if endpoint=='first' else 'c')*64
        if not authenticated and (variant!='guest' or path=='/api/v1/game/scores' or self.headers.get('Authorization')):
            return self.reply({},401)
        if path=='/api/v1/game/bootstrap':
            # A higher old-version score must never overwrite the current score.
            scores=[score(endpoint),score(endpoint,id='old',versionId='d'*32,score=9999999)]
            if variant=='missing-combo': del scores[0]['max_combo']
            if variant=='null-combo': scores[0]['max_combo']=None
            if not authenticated: scores=[]
            if variant=='refresh': time.sleep(0.15)
            categories=[
                {'id':'game','title':'Game','genre':'GAME','chartCount':1},
                {'id':'pop','title':'Pop','genre':'J-POP','chartCount':1},
                {'id':'anime','title':'Anime','genre':'Anime' if endpoint=='second' else 'ANIME','chartCount':1},
                {'id':'variety','title':'Variety','genre':'VARIETY','chartCount':0}]
            if variant=='refresh' and counts['refresh:'+path]>1:
                categories.append({'id':'classic','title':'Classic','genre':'CLASSICAL','chartCount':0})
            snapshot={'categories':categories,'chartCount':1,'scores':scores}
            if endpoint=='first': snapshot['scoreReplayVersion']=1
            return self.reply(snapshot)
        if path.startswith('/api/v1/game/categories/'):
            category=path.split('/')[-2]
            if category=='variety':
                return self.reply({'categoryId':category,'charts':[]})
            if variant=='refresh':
                attempt=counts['refresh:/api/v1/game/categories/game/charts']
                if category=='pop' and attempt==2: return self.reply({},503)
                if attempt>=2:
                    changed=chart(endpoint); changed['id']='2'*32
                    songs=[chart(endpoint),changed] if category=='game' else []
                    return self.reply({'categoryId':category,'charts':songs})
            return self.reply({'categoryId':category,'charts':[chart(endpoint)]})
        if path=='/api/v1/charts/'+SONG: return self.reply(chart(endpoint))
        if path.endswith('/tja'): return self.transfer(TJA)
        if path.endswith('/audio'): return self.transfer(AUDIO, known_length=endpoint!='second')
        if path=='/api/v1/game/scores':
            body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            assert body['difficulty']=='Oni' and body['versionId']==VERSION
            assert [body[x] for x in ['good','ok','bad','score','drumroll','max_combo']]==[12,3,1,999999,9,11]
            if endpoint=='first':
                assert body['replay_data']==dict(version=1,audio_offset_ms=-20,visual_offset_ms=10,
                    inputs=[[-15.5,0],[1234.5,1],[1234.5,2],[1200,3]]), 'recording changed or lost simultaneous inputs'
            else:
                assert 'replay_data' not in body, 'legacy servers reject unknown recording fields'
            key=(endpoint,self.headers.get('Idempotency-Key'))
            assert len(key[1])==64
            with lock:
                retry=key in stored
                if retry: assert stored[key]==body
                stored[key]=body
            if not retry and endpoint=='first': return self.reply({"code":"TEMPORARY_FAILURE_AFTER_COMMIT"},500)
            return self.reply(score(endpoint,**body,id=key[1]),201)
        return self.reply({},404)

if __name__=='__main__':
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True); thread.start()
    base=f'http://127.0.0.1:{server.server_port}'
    try:
        with tempfile.TemporaryDirectory(prefix='fanmade-fixture-') as cache:
            env={**os.environ,'http_proxy':'http://127.0.0.1:1','https_proxy':'http://127.0.0.1:1','ALL_PROXY':'http://127.0.0.1:1','NO_PROXY':'*'}
            subprocess.run([sys.argv[1],base,cache],env=env,check=True)
            assert counts['proxy']>0, 'configured proxy unused'
            assert sum(v for k,v in counts.items() if k.endswith('/tja') and not k.startswith('guest:'))==3, counts
            assert sum(v for k,v in counts.items() if k.endswith('/audio') and not k.startswith('guest:'))==4, counts
            assert counts['guest:/api/v1/game/login']==2, counts
            assert counts['guest:/api/v1/game/scores']==0, counts
            assert counts['guest:/api/v1/charts/'+SONG+'/versions/'+VERSION+'/tja']==4, counts
            assert counts['guest:/api/v1/charts/'+SONG+'/versions/'+VERSION+'/audio']==4, counts
            assert len(stored)==2, stored
            assert not any('replay' in route for route in counts), 'client must not request or poll replays'
            print('PASS: guest playback, no guest login/score upload, proxy routing, exact downloads, version isolation, no DOUBLE upload')
    finally: server.shutdown()
