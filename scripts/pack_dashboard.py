"""Bundle only public editor assets into the firmware; never include user data."""
import gzip,re,sys,json
from pathlib import Path

def automata_config(root):
    c=json.loads((root.parent/'cellular-automata-config.json').read_text(encoding='utf-8'))
    for key,low,high in [('time_reverse_frames',0,120),('update_ms',1,60000),('time_hold_seconds',0,59),('clear_every_minutes',1,60),('fade_after_seconds',0,86400),('fade_duration_seconds',1,86400)]:
        if type(c.get(key)) is not int or not low<=c[key]<=high:raise ValueError('Invalid cellular automata setting: '+key)
    if c['time_reverse_frames']*c['update_ms']+c['time_hold_seconds']*1000>60000:raise ValueError('Time animation lead-in plus hold must fit within one minute')
    if 60%c['clear_every_minutes']:raise ValueError('clear_every_minutes must divide 60')
    if c.get('gradient') not in ('none','horizontal','vertical','both'):raise ValueError('Invalid gradient direction')
    for key in ('color','gradient_end_color'):
        if not isinstance(c.get(key),str) or not re.fullmatch('#[0-9a-fA-F]{6}',c[key]):raise ValueError('Invalid color: '+key)
    return {key:value for key,value in c.items() if key!='_comments'}

def pack(root,destination):
    config=automata_config(root)
    settings=['/* Generated from cellular-automata-config.json. */','#pragma once']
    for key in ('time_reverse_frames','update_ms','time_hold_seconds','clear_every_minutes','fade_after_seconds','fade_duration_seconds'):settings.append('#define CA_'+key.upper()+' '+str(config[key]))
    for key in ('color','gradient_end_color'):settings.append('#define CA_'+key.upper()+' 0x'+config[key][1:]+'U')
    settings.append('#define CA_GRADIENT '+str(('none','horizontal','vertical','both').index(config['gradient'])))
    Path(destination).with_name('cellular_automata_settings.h').write_text('\n'.join(settings),encoding='utf-8')
    names=['index.html']
    entries=[];source=['/* Generated public dashboard files. */','#include <stddef.h>','typedef struct {const char *path,*type;const unsigned char *data;size_t size;} dashboard_asset_t;']
    for i,name in enumerate(names):
        raw=(root/name).read_text(encoding='utf-8')
        if name=='index.html':
            raw=raw.replace('shared-workspace.js','board-workspace.js')
            raw=re.sub(r'<button id="ai-open".*?</button>','',raw)
            raw=re.sub(r'<dialog id="ai-dialog">.*?</dialog>','',raw,flags=re.S)
            raw=re.sub(r'<button id="deploy-open".*?</button>','',raw)
            raw=re.sub(r'<dialog id="deploy-dialog">.*?</dialog>','',raw,flags=re.S)
            raw=raw.replace('USB deployment samples animations and keeps clock/date live.','Edits appear live over Wi-Fi and save automatically on this clock.')
            # A single page avoids parallel asset downloads exhausting Wi-Fi RAM.
            raw=re.sub(r'<script defer src="[^"]+"></script>','',raw)
            raw=re.sub(r'<link rel="stylesheet" href="[^"]+">','',raw)
            css='\n'.join((root/n).read_text(encoding='utf-8') for n in ['styles.css','studio.css','designer.css'])
            raw=raw.replace('</head>','<style>'+css+'</style></head>')
            core=['engine.js','widget-data.js','animation.js','scene-weather.js','minute-birds.js','island-assets.js','cellular-automata.js','scene.js','studio-state.js']
            scripts=''.join('<script>'+(root/n).read_text(encoding='utf-8').replace('</script','<\\/script')+'</script>' for n in core)
            boots=','.join('function(){'+(root/n).read_text(encoding='utf-8').replace('</script','<\\/script')+'}' for n in ['app.js','designer.js','weather-panel.js','widget-editor.js'])
            scripts+='<script>CellularAutomata.configure('+json.dumps(config)+');window.BoardEditorBoot=['+boots+'];</script><script>'+(root/'board-workspace.js').read_text(encoding='utf-8')+'</script>'
            raw=raw.replace('</body>',scripts+'</body>')
        data=gzip.compress(raw.encode('utf-8'),mtime=0)
        mime='text/html' if name.endswith('.html') else 'text/css' if name.endswith('.css') else 'text/javascript'
        source.append(f'static const unsigned char asset_{i}[]={{'+','.join(str(b) for b in data)+'};')
        entries.append(f'{{"/{name}","{mime}; charset=utf-8",asset_{i},sizeof(asset_{i})}}')
    source.append('static const dashboard_asset_t dashboard_assets[]={'+','.join(entries)+'};')
    Path(destination).write_text('\n'.join(source),encoding='utf-8')
    return names

if __name__=='__main__':pack(Path(sys.argv[1]),sys.argv[2])
