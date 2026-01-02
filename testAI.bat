set "pathTo=build/apps/Console/RelWithDebInfo/"

REM MonteCarloAI(3000)
REM MCTS_Deterministic(10000;4)
REM MCTS_Zero(15000;4;[TwoLayers16_PUCT];[zero];[C][scienceBoost])

REM "%pathTo%\Console.exe" --mode stats --in zero10K
REM pause

REM "%pathTo%\Console.exe" --mode train --net TwoLayer8 --gen 0 --epochs 64 --in pureMC1000 --out pureMC1000

REM "%pathTo%\Console.exe" --mode generate --size 50 --ai="MCTS_Deterministic(10000;4)" --ai="MonteCarloAI(3000)" --out tmp
REM "%pathTo%\Console.exe" --mode generate --size 48 --ai="MCTS_Zero(5000;4)" --ai="MCTS_Deterministic(5000;4)" --ai="MonteCarloAI(3000)" --out tmp --threads 12
REM "%pathTo%\Console.exe" --mode generate --size 48 --ai="MCTS_Zero(10000;4;TwoLayers16_PUCT;zero)" --ai="MCTS_Deterministic(5000;4)" --ai="MonteCarloAI(3000)" --out tmp --threads 12

"%pathTo%\Console.exe" --mode generate --size 400 ^
--ai="MCTS_Zero(10000;8;TwoLayers16_PUCT;zeroSRL)" ^
--ai="MCTS_Zero(10000;8;TwoLayers16_PUCT;zeroSRL_2)" ^
--ai="MCTS_Zero(10000;8;TwoLayers32_PUCT;tl32)" ^
--out tmp --threads 12

REM "%pathTo%\Console.exe" --mode generate --size 64 --ai="MCTS_Deterministic(5000;4;TwoLayers8;test)" --ai="MCTS_Deterministic(5000;4;TwoLayers8;test)" --out tmp

pause
