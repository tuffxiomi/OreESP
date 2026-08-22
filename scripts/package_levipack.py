import argparse, json, zipfile
from pathlib import Path

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--library',required=True,type=Path)
    ap.add_argument('--icon',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args()
    manifest={
        'type':'preload-native',
        'name':'Ore ESP',
        'author':'xiomi',
        'description':'Highlights nearby ore blocks with bright outlines.',
        'version':'1.0.0',
        'entry':'libOreESP.so',
        'icon':'icon.png',
        'overwrite_files':[],
        'overwrite_folders':[]
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        z.writestr('manifest.json',json.dumps(manifest,indent=2)+'\n')
        z.write(args.library,'libOreESP.so')
        z.write(args.icon,'icon.png')
    with zipfile.ZipFile(args.output) as z:
        names=set(z.namelist())
        assert names=={'manifest.json','libOreESP.so','icon.png'}
    print(args.output)

if __name__=='__main__': main()
