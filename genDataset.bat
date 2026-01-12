set "pathTo=build/apps/Console/RelWithDebInfo/"
set "NumThreads=12"
set "AI_NumNode=50000"
set "AI_NumSimu=8"

REM Transfert existing dataset
REM "%pathTo%\Console.exe" --mode generate --size 1 --ai="MCTS_Deterministic(20000;4)" --ai="MCTS_Deterministic(10000;8)" --in pureMC1000 --out mixed

set /a i=0

:loop
set /a i+=1
echo Iteration %i%

"%pathTo%\Console.exe" --mode generate --size 36 ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;tl_16;2.0)" ^
--ai="MCTS_Zero(%AI_NumNode%;%AI_NumSimu%;TwoLayers16_PUCT;tl_16;2.0)" ^
--in bigFinal2 --out bigFinal2 --threads %NumThreads%


goto loop
