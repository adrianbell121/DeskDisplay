"""Bundle only public editor assets into the firmware; never include user data."""
import gzip,re,sys
from pathlib import Path

def pack(root,destination):
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
            core=['engine.js','widget-data.js','animation.js','scene-weather.js','minute-birds.js','island-assets.js','scene.js','studio-state.js']
            scripts=''.join('<script>'+(root/n).read_text(encoding='utf-8').replace('</script','<\\/script')+'</script>' for n in core)
            boots=','.join('function(){'+(root/n).read_text(encoding='utf-8').replace('</script','<\\/script')+'}' for n in ['app.js','designer.js','weather-panel.js','widget-editor.js'])
            scripts+='<script>window.BoardEditorBoot=['+boots+'];</script><script>'+(root/'board-workspace.js').read_text(encoding='utf-8')+'</script>'
            raw=raw.replace('</body>',scripts+'</body>')
        data=gzip.compress(raw.encode('utf-8'),mtime=0)
        mime='text/html' if name.endswith('.html') else 'text/css' if name.endswith('.css') else 'text/javascript'
        source.append(f'static const unsigned char asset_{i}[]={{'+','.join(str(b) for b in data)+'};')
        entries.append(f'{{"/{name}","{mime}; charset=utf-8",asset_{i},sizeof(asset_{i})}}')
    source.append('static const dashboard_asset_t dashboard_assets[]={'+','.join(entries)+'};')
    Path(destination).write_text('\n'.join(source),encoding='utf-8')
    return names

if __name__=='__main__':pack(Path(sys.argv[1]),sys.argv[2])
