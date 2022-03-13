set title "experiment of K value"
set xlabel "log_2N"
set logscale x 2
set format x "{%L}"
set ylabel "K"
set terminal png font " Times_New_Roman,12 "
set output "k_optimal_plot.png"
set key bottom 

plot \
'k_sort.txt' using 1:($2) with linespoints linewidth 0.1 pointtype 0 title "my sort", \
'k_kernel.txt' using 1:($2) with linespoints linewidth 0.1 pointtype 0 title "kernel sort"
