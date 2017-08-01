#set terminal postscript eps solid color
#set output "cpu_all.eps"

set multiplot layout 1,2
set xtics rotate
#set bmargin 5
#set size 1,0.5

load 'palette_new'
set style data histograms
#set key outside center top horizontal Left reverse autotitle nobox
#set key outside center top horizontal Left reverse autotitle nobox samplen 1 at screen 0.5,screen 0.53
#set key outside center top horizontal Left reverse autotitle nobox samplen 1
unset key
set datafile missing '-'

set style fill solid 1.00 border lt -1
set style histogram clustered gap 1 title textcolor lt -1
#set xtics center border in scale 0,0 mirror rotate by -270  offset character 0, 0, 0 autojustify
#set xtics norangelimit
#set xtics ()
unset xtics

#set xtics border in scale 0,0 nomirror autojustify offset -0.3,graph 0.02 in
#set xtics norangelimit enhanced
set ytics border in scale 0,0 nomirror autojustify
#set bmargin 3
#set rmargin 5

#set title "KPIs for different Downsizing Factors - Tree topology - M=1e7, R=1e7" 

#set xlabel  
#set ylabel 'P_{hit} [%]' enhanced offset 1.2,0,0
#set y2label 'Accuracy Loss [%]' enhanced offset 0,0,0
#set title 'P_{hit} [%]'

#set yrange [1:70000]
set yrange [1:55000000]
set log y 2
#set ytics ("50000" 50000, "5000" 5000, "500" 500, "50" 50, "0" 0.1)
unset ytics
#set ytics () nomirror
#set mytics 2
#set format y "%.2t*10^%+03T"
set format y "%.2e"
#set y2range [0:4]
#set grid ytics lt 1 lw 1 lc rgb "black"
#set grid y2tics lt 1 lw 1 lc rgb "black"

#set border 3 front ls 1 lc rgb "black"
set border 3 front ls 1 lc rgb "black"




#set xtics ("1.0" 0, "0.95" 1, "0.9" 2, "0.75" 3, "0.5" 4)
#set xtics ("ED" 0, "ModelGraft\nReqSpl" 1, "ModelGraft\nReqSha" 2, "CS\nReqSha" 3)
#set ytics ("1.0" -1, "0" 0, "1.0" 1)
set boxwidth 0.7 relative 

unset colorbox

set size 0.5,0.5
set orig 0.0,0.0

#plot 'DATA/accuracy_all_methods.dat' using 2 ti col lt 8 fs pattern 5
#plot 'DATA/accuracy_all_methods.dat' using 2 ti col lt 6 fs pattern 2

#plot 'DATA/accuracy_all_methods_1e9.dat' using 0:4:3 w boxes lc palette
#plot 'data_1e12.dat' using 0:4:3 w boxes lc palette, '' u 0:($4 + 10):($4) with labels
plot 'data_1e12.dat' using 0:4:3 w boxes lc palette
#plot 'DATA/accuracy_all_methods.dat' using 2:xtic(1) ti col

#using 6:xtic(1) ti col, '' u 12 ti col, '' u 13 ti col, '' u 14 ti col

####### 1e10 ########
#set title 'Memory [MB]'
unset arrow
unset obj
unset label
unset key 
unset xtics

#set yrange [1:550000]
#set yrange [0.1:8000]
#set log y
#set ytics ("8000" 8000, "1000" 1000, "50" 50, "0" 0.1)

set size 0.5,0.5
set orig 0.5,0.0

#set label "6371" at -0.25,11000
#set label "38" at  0.855,80
#set label "23" at  1.855,80
#set label "829" at  2.755,1500

#plot 'DATA/accuracy_all_methods_1e10.dat' using 0:4:3 w boxes lc palette
plot 'data_1e12.dat' using 0:5:3 w boxes lc palette, '' u 0:($4 + 10):($4) with labels
#plot 'data_1e12.dat' using 0:5:3 w boxes lc palette