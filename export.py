import sys
import os.path
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import export_obj
import bpy
from bpy_extras.io_utils import axis_conversion

 
argv = sys.argv
argv = argv[argv.index("--") + 1:] # get all args after "--"
 
obj_out = argv[0]

matrix = axis_conversion(to_forward='-Z', to_up='Y').to_4x4()
export_obj.save(None, bpy.context, filepath=obj_out, use_triangles=True,
	        use_edges=False, use_normals=True, use_selection=False,
		global_matrix=matrix)
