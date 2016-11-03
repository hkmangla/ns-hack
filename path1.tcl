


#  Simulation parameters setup

set val(stop)   30                         ;# time of simulation end

#        Initialization        
#Create a ns simulator
set ns [new Simulator]
$ns use-newtrace

$ns rtproto DV


$ns color 0 blue
$ns color 1 red
$ns color 2 green

#Open the NS trace file
set tracefile [open out.tr w]
$ns trace-all $tracefile

#Open the NAM trace file
set namfile [open out.nam w]
$ns namtrace-all $namfile

#        Nodes Definition        
#Create 20 nodes
set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]
set n5 [$ns node]
set n6 [$ns node]
set n7 [$ns node]
set n8 [$ns node]
set n9 [$ns node]
set n10 [$ns node]
set n11 [$ns node]
set n12 [$ns node]
set n13 [$ns node]
set n14 [$ns node]
set n15 [$ns node]
set n16 [$ns node]
set n17 [$ns node]
set n18 [$ns node]
set n19 [$ns node]
set n20 [$ns node]

$n1 shape "square"
$n1 color "blue"   

$n10 shape "square"
$n10 color "blue"

#Createlinks between nodes
$ns duplex-link $n14 $n0 30Mb 2ms DropTail
$ns queue-limit $n14 $n0 50
$ns duplex-link $n0 $n1 30Mb 2ms DropTail
$ns queue-limit $n0 $n1 50
$ns duplex-link $n1 $n2 30Mb 2ms DropTail
$ns queue-limit $n1 $n2 50
$ns duplex-link $n2 $n3 30Mb 2ms DropTail
$ns queue-limit $n2 $n3 50
$ns duplex-link $n3 $n4 30Mb 2ms DropTail
$ns queue-limit $n3 $n4 50
$ns duplex-link $n4 $n5 30Mb 2ms DropTail
$ns queue-limit $n4 $n5 50
$ns duplex-link $n5 $n6 30Mb 2ms DropTail
$ns queue-limit $n5 $n6 50
$ns duplex-link $n6 $n7 30Mb 2ms DropTail
$ns queue-limit $n6 $n7 50
$ns duplex-link $n7 $n8 30Mb 2ms DropTail
$ns queue-limit $n7 $n8 50
$ns duplex-link $n8 $n9 30Mb 2ms DropTail
$ns queue-limit $n8 $n9 50
$ns duplex-link $n9 $n10 30Mb 2ms DropTail
$ns queue-limit $n9 $n10 50
$ns duplex-link $n10 $n11 30Mb 2ms DropTail
$ns queue-limit $n10 $n11 50
$ns duplex-link $n11 $n12 30Mb 2ms DropTail
$ns queue-limit $n11 $n12 50
$ns duplex-link $n12 $n13 30Mb 2ms DropTail
$ns queue-limit $n12 $n13 50
$ns duplex-link $n13 $n14 30Mb 2ms DropTail
$ns queue-limit $n13 $n14 50
$ns duplex-link $n0 $n15 30Mb 2ms DropTail
$ns queue-limit $n0 $n15 50
$ns duplex-link $n15 $n16 30Mb 2ms DropTail
$ns queue-limit $n15 $n16 50
$ns duplex-link $n16 $n12 30Mb 2ms DropTail
$ns queue-limit $n16 $n12 50
$ns duplex-link $n1 $n17 30Mb 2ms DropTail
$ns queue-limit $n1 $n17 50
$ns duplex-link $n17 $n12 30Mb 2ms DropTail
$ns queue-limit $n17 $n12 50
$ns duplex-link $n1 $n18 30Mb 2ms DropTail
$ns queue-limit $n1 $n18 50
$ns duplex-link $n18 $n20 30Mb 2ms DropTail
$ns queue-limit $n18 $n20 50
$ns duplex-link $n4 $n19 30Mb 2ms DropTail
$ns queue-limit $n4 $n19 50
$ns duplex-link $n19 $n20 30Mb 2ms DropTail
$ns queue-limit $n19 $n20 50
$ns duplex-link $n20 $n10 30Mb 2ms DropTail
$ns queue-limit $n20 $n10 50

$ns duplex-link-op $n1 $n17 color "blue"
$ns duplex-link-op $n17 $n12 color "blue"
$ns duplex-link-op $n12 $n11 color "blue"
$ns duplex-link-op $n11 $n10 color "blue"

$ns duplex-link-op $n1 $n18 label "Failure"



$ns at 0.0 "$n1 label Source"
$ns at 0.0 "$n10 label Destination"

#Give node position (for NAM)
$ns duplex-link-op $n14 $n0 orient right-up
$ns duplex-link-op $n0 $n1 orient right
$ns duplex-link-op $n1 $n2 orient right
$ns duplex-link-op $n2 $n3 orient right
$ns duplex-link-op $n3 $n4 orient right
$ns duplex-link-op $n4 $n5 orient right
$ns duplex-link-op $n5 $n6 orient right
$ns duplex-link-op $n6 $n7 orient right-down
$ns duplex-link-op $n7 $n8 orient left-down
$ns duplex-link-op $n8 $n9 orient left

$ns duplex-link-op $n9 $n10 orient left

$ns duplex-link-op $n10 $n11 orient left

$ns duplex-link-op $n11 $n12 orient left

$ns duplex-link-op $n12 $n13 orient left

$ns duplex-link-op $n13 $n14 orient left-up
$ns duplex-link-op $n0 $n15 orient right-down
$ns duplex-link-op $n15 $n16 orient right-down
$ns duplex-link-op $n16 $n12 orient right-down
$ns duplex-link-op $n1 $n17 orient left-down
$ns duplex-link-op $n17 $n12 orient left-down
$ns duplex-link-op $n1 $n18 orient right-down
$ns duplex-link-op $n18 $n20 orient right-down
$ns duplex-link-op $n4 $n19 orient left-down
$ns duplex-link-op $n19 $n20 orient right-down
$ns duplex-link-op $n20 $n10 orient right-down

#        Agents Definition 
       
set udp1 [new Agent/UDP]
$ns attach-agent $n1 $udp1
$udp1 set class_ 1

set null1 [new Agent/Null]
$ns attach-agent $n10 $null1


#        Applications Definition        

set cbr1 [new Application/Traffic/CBR]
$cbr1 attach-agent $udp1
$ns connect $udp1 $null1







#        Termination
        
$ns at 0.1 "$cbr1 start"
$ns rtmodel-at 0.1 down $n1 $n18
$ns at 4.5 "$cbr1 stop"
$ns at 5.0 "finish"

#Define a 'finish' procedure
proc finish {} {
    global ns tracefile namfile
    $ns flush-trace
    close $tracefile
    close $namfile
    exec nam out.nam &
exec awk -f Instnt_Del.awk path1.tr > Instnt.xg &
exec xgraph Instnt.xg &
    exit 0
}
$ns at $val(stop) "$ns nam-end-wireless $val(stop)"
$ns at $val(stop) "finish"
$ns at $val(stop) "puts \"done\" ; $ns halt"
$ns run
