import re
s=open('src/lua/luascripts/challenges.lua').read()
cats=re.findall(r'\{ id = "(\w+)", name = "([^"]+)" \}', s)
defs=[]
for m in re.finditer(r'\nadd\{\n(.*?)\n\}\n', s, re.S):
    body=m.group(1)
    g=lambda k: (re.search(k+r' = "([^"]*)"', body) or [None,None])[1]
    d=dict(id=g('id'), cat=g('cat'), title=g('title'), text=g('text'), order=len(defs))
    d['diff']=int(re.search(r'diff = (\d)', body).group(1))
    tools=re.search(r'tools = \{([^}]*)\}', body).group(1)
    d['tools']=[t.strip().strip('"') for t in tools.split(',') if t.strip()]
    ink=re.search(r'\bink = (\d+)', body); d['ink']=ink.group(1) if ink else 'no limit'
    tm=re.search(r'\btime = (\d+)', body); sv=re.search(r'survive = (\d+)', body)
    d['time']= (sv.group(1)+' s to hold') if sv else ((tm.group(1)+' s') if tm else '60 s')
    defs.append(d)
def tn(t):
    if ':' in t:
        k,r=t.split(':'); return {'WALL':r+' wall','TOOL':r+' tool','LIFE':r+' (Life)'}[k]
    return t
out=['# Challenges','','The challenge mode (Challenges… on the page, or the ★ button at the bottom of the second side-bar column',
'in the game) has %d challenges in %d categories. Each one builds a scene, offers only the listed tools (the' % (len(defs), len(cats)),
'eraser always works, but not on the locked parts of the scene), limits how many particles you may place',
'(ink), and checks its goal after every frame. Stars: up to 3 for time and ink, minus one per hint used.',
'The definitions, each with a reference solution that the browser tests play, are in',
'[`src/lua/luascripts/challenges.lua`](../src/lua/luascripts/challenges.lua); this list is generated from it.','']
for cid,cname in cats:
    mine=sorted([d for d in defs if d['cat']==cid], key=lambda d:(d['diff'],d['order']))
    out.append('## %s (%d)' % (cname, len(mine))); out.append('')
    out.append('| Challenge | Difficulty | Goal | Tools | Ink | Time |'); out.append('|---|---|---|---|---|---|')
    for d in mine:
        out.append('| %s | %s | %s | %s | %s | %s |' % (d['title'], '●'*d['diff']+'○'*(5-d['diff']), d['text'], ', '.join(tn(t) for t in d['tools']), d['ink'], d['time']))
    out.append('')
open('docs/CHALLENGES.md','w').write('\n'.join(out))
print(len(defs))
