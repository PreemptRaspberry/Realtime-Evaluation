# Results from running `cyclictest` on all our 4 configurations.

All tests were made with 100000000 cycles, so they take ~5h

Nice hint: `ssh raspberry '(date > start.log; logLastingCommand && date > end.log && sudo shutdown) >&- 2>&- <&- &'`
to run this in backgound and go to bed :)
