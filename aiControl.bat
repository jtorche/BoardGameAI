set "pathTo=build/apps/Console/RelWithDebInfo/"
"%pathTo%Console.exe" --mode generate --size 10 --ai=MCTS_Deterministic(10000;8) --ai=MonteCarloAI(1000) --in test_ --out test_
pause