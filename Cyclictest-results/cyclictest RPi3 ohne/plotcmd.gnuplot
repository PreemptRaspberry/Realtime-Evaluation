set title "Cyclictest Raspberry Pi 3"
set terminal pdf font "Nimbus Roman"
set xlabel "Latency (us), max 00468 us"
set logscale y
set xrange [0:120]
set yrange [0.8:*]
set ylabel "Number of latency samples"
set output "3.pdf"
plot "histogram" using 1:2 title "CPU0" with histeps, \
     "histogram" using 1:3 title "CPU1" with histeps, \
     "histogram" using 1:4 title "CPU2" with histeps, \
     "histogram" using 1:5 title "CPU3" with histeps
