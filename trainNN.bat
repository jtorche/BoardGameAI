set "pathTo=build/apps/Console/RelWithDebInfo/"

REM "%pathTo%\Console.exe" --mode train --net TwoLayers8_PUCT --extra --gen 0 --epochs 32 --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in zero --out test
REM "%pathTo%\Console.exe" --mode train --net TwoLayers16_PUCT --extra --gen 0 --epochs 40 --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in zero10K --out zero
"%pathTo%\Console.exe" --mode train --net TwoLayers64_PUCT --extra --gen 0 --epochs 40 --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in zero10K --out zero
REM "%pathTo%\Console.exe" --mode train --net TwoLayers24 --gen 0 --epochs 64 --batch="64;64;64" --alpha="0.0002;0.0002;0.0002" --in zero --out test
REM "%pathTo%\Console.exe" --mode train --net TwoLayers8 --gen 0 --epochs 64 --batch="128;128;128" --alpha="0.0003;0.0003;0.0003" --in zero --out test
pause