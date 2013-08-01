#!/bin/sh
if [ "$1" = "--anim" ]; then
  blender "$2" --background --python export_anim.py -- "$3"
  sed -i 's,[^ ]*/\([^ /]*\),\1,g' data/$(basename "$3" .obj)_*.mtl
else
  blender "$1" --background --python export.py -- "$2"
  sed -i 's,[^ ]*/\([^ /]*\),\1,g' "data/$(basename "$2" .obj).mtl"
fi
