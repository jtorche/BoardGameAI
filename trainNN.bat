set "pathTo=build/apps/Console/RelWithDebInfo/"

"%pathTo%\Console.exe" --mode train --net TwoLayer8 --gen 0 --epochs 32 --in pureMC1000 --out pureMC1000
pause