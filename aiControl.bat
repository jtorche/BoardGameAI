set "pathTo=build/apps/Console/RelWithDebInfo/"
set /a i=0

:loop
set /a i+=1
echo Iteration %i%
REM "%pathTo%\Console.exe" --mode generate --size 100 --ai="MonteCarloAI(10000)" --ai="MonteCarloAI(10000)" --in pureMC1000_ --out pureMC1000_
"%pathTo%\Console.exe" --mode generate --size 100 --ai="MonteCarloAI(100)" --ai="MonteCarloAI(100)" --in test --out test
pause
goto loop
