set "pathTo=build/apps/Console/RelWithDebInfo/"
set "InitialDataset=zero10K"
set "NumThreads=8"
set "NumEpochs=32"
set "NumGames=8000"
set "PreviousAI=zeroSRL"	
set "NextAI=zeroSRL_2"	

set "AI_NumNode=10000"
set "AI_NumSimu=4"

REM Initial generation with previous "strong" AI
"%pathTo%\Console.exe" --mode generate --size %NumGames% ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%PreviousAI%)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%PreviousAI%)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%PreviousAI%;2.0;0.25)" ^
--out %NextAI% --threads %NumThreads%

"%pathTo%\Console.exe" --mode train --net TwoLayers16_PUCT --extra --gen 0 --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in %NextAI% --out %NextAI%

set /a i=0

:loop
set /a i+=1
echo Iteration %i%

"%pathTo%\Console.exe" --mode generate --size %NumGames% ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%NextAI%)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%NextAI%)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%NextAI%;2.0;0.25)" ^
--out %NextAI% --threads %NumThreads%

"%pathTo%\Console.exe" --mode train --net TwoLayers16_PUCT --extra --gen %i% --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in %NextAI% --out %NextAI%

echo -------------------------------------
echo -- Little match with previous AI:
echo -------------------------------------
"%pathTo%\Console.exe" --mode generate --size 32 --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers8_PUCT;%PreviousAI%)" --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%NextAI%)" --out tmp --threads %NumThreads%
"%pathTo%\Console.exe" --mode generate --size 32 --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%PreviousAI%)" --ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%NextAI%)" --out tmp --threads %NumThreads%
echo -------------------------------------
echo -- Start next generation:
echo -------------------------------------

goto loop