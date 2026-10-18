"""Headless check for Level 0: runs Tools/verify_level.py --theme level0.
Run: UnrealEditor-Cmd.exe LevelZero.uproject -run=pythonscript -script="<this file>" """
import os
import sys
import unreal

_here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools')
sys.argv = [os.path.join(_here, 'verify_level.py'), '--theme', 'level0']
exec(compile(open(os.path.join(_here, 'verify_level.py')).read(), 'verify_level.py', 'exec'), {'__name__': '__main__'})
