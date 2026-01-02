set "pathTo=build/apps/Console/RelWithDebInfo/"
set "NumThreads=8"
set "NumGames=1000"
set "AI_NumNode=10000"
set "AI_NumSimu=8"

REM Transfert existing dataset
REM "%pathTo%\Console.exe" --mode generate --size 1 --ai="MCTS_Deterministic(20000;4)" --ai="MCTS_Deterministic(10000;8)" --in pureMC1000 --out mixed

set /a i=0

:loop
set /a i+=1
echo Iteration %i%

"%pathTo%\Console.exe" --mode generate --size 1000 ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;zeroBest;2.0)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;zeroBest;2.0;0.1)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;zeroBest;2.0;0.2)" ^
--in bigFinal --out bigFinal --threads %NumThreads%


goto loop
