set "pathTo=build/apps/Console/RelWithDebInfo/"
set "InitialDataset=zero10K"
set "NumThreads=8"
set "NumEpochs=32"
set "NumGames=8000"

set "AI_NumNode=5000"
set "AI_NumSimu=4"

echo Skip initial training. (yes/no)
set /p choice=

IF /i "%choice%"=="yes" goto afterInit

REM Initial baseline training from starting dataset
"%pathTo%\Console.exe" --mode stats --in %InitialDataset%
pause

"%pathTo%\Console.exe" --mode train --net TwoLayers8_PUCT --extra --gen 0 --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in %InitialDataset% --out baseline

REM Initial gen0 training from starting dataset
"%pathTo%\Console.exe" --mode train --net TwoLayers8_PUCT --extra --gen 0 --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in %InitialDataset% --out zeroSRL
"%pathTo%\Console.exe" --mode train --net TwoLayers16_PUCT --extra --gen 0 --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in %InitialDataset% --out zeroSRL

:afterInit

set /a i=0

:loop
set /a i+=1
echo Iteration %i%

"%pathTo%\Console.exe" --mode generate --size %NumGames% ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;zeroSRL)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;zeroSRL)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;zeroSRL;2.0;0.33)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;zeroSRL;2.0;0.66)" ^
--out zeroSRL --threads %NumThreads%

"%pathTo%\Console.exe" --mode train --net TwoLayers8_PUCT --extra --gen %i% --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in zeroSRL --out zeroSRL
"%pathTo%\Console.exe" --mode train --net TwoLayers16_PUCT --extra --gen %i% --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in zeroSRL --out zeroSRL

echo -------------------------------------
echo -- Little match with baseline AI:
echo -------------------------------------
"%pathTo%\Console.exe" --mode generate --size 32 --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;baseline)" --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;zeroSRL)" --out tmp --threads %NumThreads%
"%pathTo%\Console.exe" --mode generate --size 32 --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;baseline)" --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;zeroSRL)" --out tmp --threads %NumThreads%
echo -------------------------------------
echo -- Start next generation:
echo -------------------------------------

goto loop