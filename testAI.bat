set "pathTo=build/apps/Console/RelWithDebInfo/"

REM "%pathTo%\Console.exe" --mode train --net TwoLayer8 --gen 0 --epochs 64 --in pureMC1000 --out pureMC1000

REM "%pathTo%\Console.exe" --mode generate --size 50 --ai="MCTS_Deterministic(10000;4)" --ai="MonteCarloAI(3000)" --out tmp
"%pathTo%\Console.exe" --mode generate --size 32 --ai="MCTS_Deterministic(10000;8;NoBurn)" --ai="MCTS_Deterministic(10000;8)" --out tmp

pause
