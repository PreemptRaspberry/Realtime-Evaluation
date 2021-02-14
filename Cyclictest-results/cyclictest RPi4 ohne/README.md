**1** - `sudo apt install rt-tests`
**2** - `cyclictest -l100000000 -m -Sp90 -i200 -h400 -q > cyclictest-rpi4-ohne.txt`
**3** - `max=$(grep "Max Latencies" cyclictest-rpi4-ohne.txt | tr " " "\n" | sort -n | tail -1)`
**4** - `grep -v -e "^#" -e "^$" cyclictest-rpi4-ohne.txt | tr " " "\t" >histogram`
```
echo -n -e "set title \"Cyclictest Raspberry Pi 4\"\n\
set terminal png\n\
set xlabel \"Latency (us), max $max us\"\n\
set logscale y\n\
set xrange [0:(($max+100))]\n\
set yrange [0.8:*]\n\
set ylabel \"Number of latency samples\"\n\
set output \"plot.png\"\n\
plot "histogram" using 1:2 title "CPU0" with histeps, \n\
     "histogram" using 1:3 title "CPU1" with histeps, \n\
     "histogram" using 1:4 title "CPU2" with histeps, \n\
     "histogram" using 1:5 title "CPU3" with histeps" >plotcmd
```
**5** `gnuplot -persist < plotcmd`
