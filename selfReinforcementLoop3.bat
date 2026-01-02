set "pathTo=build/apps/Console/RelWithDebInfo/"
set "NumThreads=16"
set "NumEpochs=32"
set "NumGames=25000"
set "PreviousAI=zeroSRL_2"	
set "NextAI=zeroSRL_3"	

set "AI_NumNode=20000"
set "AI_NumSimu=8"

"%pathTo%\Console.exe" --mode generate --size %NumGames% ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%PreviousAI%)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;%PreviousAI%)" ^
--out %NextAI% --threads %NumThreads%

"%pathTo%\Console.exe" --mode train --net TwoLayers16_PUCT --extra --gen 0 --epochs %NumEpochs% --batch="128;128;128" --alpha="0.0001;0.0001;0.0001" --in %NextAI% --out %NextAI%