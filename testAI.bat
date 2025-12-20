set "pathTo=build/apps/Console/RelWithDebInfo/"

REM "%pathTo%\Console.exe" --mode train --net TwoLayer8 --gen 0 --epochs 64 --in pureMC1000 --out pureMC1000

REM "%pathTo%\Console.exe" --mode generate --size 100 --ai="MCTS_Simple(250;8;TwoLayers8;pureMC1000)" --ai="MonteCarloAI(100)" --out tmp
"%pathTo%\Console.exe" --mode generate --size 100 --ai="MCTS_Deterministic(20000;4)" --ai="MCTS_Deterministic(10000;8)" --ai "MonteCarloAI(10000)" --out tmp

pause
