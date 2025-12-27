set "pathTo=build/apps/Console/RelWithDebInfo/"

"%pathTo%\Console.exe" --mode train --net TwoLayers8 --gen 0 --epochs 64 --batch="16;64;64" --alpha="0.001;0.0001;0.0001" --in mixed --out mixed
REM "%pathTo%\Console.exe" --mode train --net TwoLayers24 --gen 0 --epochs 64 --batch 32 --alpha1 0.001 --alpha2 0.0001 --alpha3 0.0001 --in mixed --out mixed
pause