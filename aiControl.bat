set "pathTo=build/apps/Console/RelWithDebInfo/"

REM Transfert existing dataset
REM "%pathTo%\Console.exe" --mode generate --size 1 --ai="MCTS_Deterministic(20000;4)" --ai="MCTS_Deterministic(10000;8)" --in pureMC1000 --out mixed

set /a i=0

:loop
set /a i+=1
echo Iteration %i%
REM "%pathTo%\Console.exe" --mode generate --size 160 --ai="MCTS_Deterministic(200;4)" --ai="MCTS_Deterministic(100;8)" --ai="MCTS_Deterministic(200;4)" --ai="MCTS_Deterministic(100;8)" --in test --out test --threads 8
REM "%pathTo%\Console.exe" --mode generate --size 160 --ai="MCTS_Deterministic(40000;4)" --ai="MCTS_Deterministic(20000;8)" --ai="MCTS_Deterministic(40000;4)" --ai="MCTS_Deterministic(20000;8)" --in mixed --out mixed --threads 8
"%pathTo%\Console.exe" --mode generate --size 480 --ai="MCTS_Zero(1000;4)" --ai="MCTS_Zero(1000;4)" --in zero --out zero --threads 12
goto loop
