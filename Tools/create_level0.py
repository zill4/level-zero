"""Rebuilds Level 0 (/Game/LevelZero/Maps/Level0) from Tools/levels/level0.json via Tools/create_level.py.
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="<this file>" """
import os
import sys
import unreal

_here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools')
sys.argv = [os.path.join(_here, 'create_level.py'), '--theme', 'level0'] + [a for a in sys.argv[1:] if a == '--rebuild-master']
exec(compile(open(os.path.join(_here, 'create_level.py')).read(), 'create_level.py', 'exec'), {'__name__': '__main__'})
unreal.log('LZ: CREATE_LEVEL0_DONE')
