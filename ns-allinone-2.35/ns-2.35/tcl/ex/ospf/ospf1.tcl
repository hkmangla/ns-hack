
# AUTORA: Inés María Romero Dávia

# OBJETIVO
#   	PRTOCOLO DE ENCAMINAMIENTO OSPF
#
# 	Escenario para probar el protocolo de routing OSPF sobre network simulator. 
#	Las conexiones lógicas son:
#
#			n0 ->n4
#			

# TOPOLOGIA
#
#		$n0       $n3
#		   \     /   \
#		    \   /     \
#		     $n2	$n4
#		    /	\      / 
#		   /     \    / 	 
#		$n1       $n5
#
#       6 nodos 0 - 5
#       Links entre 0-2, 1-2
#               bandwith 2 Mbps    delay 2ms      DropTail
#	Links restantes
#		bandwith 1.5Mbps    delay  10ms	    DropTail

# AGENTES DE TRÁFICO
#       Agente TCP
#               Nodo 0
#       Agente null/Sink
#               Nodo 4
#       Un agente Traffic CBR para generar el trafico
#               Nodo 0 Packetsize 60    Interval 0.02      
#

# AGENTES DE ROUTING
#	Agente OSPF
#		Todos los nodos	

# COSTE ENLACES:
#  Topologia por defecto:	
#		2-5:5-2 = 2
#		5-4:4-5 = 2
#		resto = 1
#  Resto de topologías:
#		todos   = 1
#
######################################################################################
Agent/TCP set syn_ false
Agent/TCP set delay_growth_ false
# In preparation for changing the default values for syn_ and delay_growth_.

remove-all-packet-headers       ; # removes all except common
add-packet-header Flags IP TCP rtProtoOSPF ; # hdrs reqd for validation test
 
puts "(TCL) Creating simulator & trace files..."
set dir "./out_ospf1"

set ns [new Simulator]
set vj_ss true



set f [eval open $dir/ospf1.tr w]
set nf [open $dir/ospf1.nam w]
$ns trace-all $f
$ns namtrace-all $nf

proc finish {} {
	global ns f nf dir
	$ns flush-trace
	close $f
	close $nf
	eval exec nam $dir/ospf1.nam 
  puts "(TCL) Finishing..."
	exit 0
}



#################################################
#  NODES
#################################################
puts "(TCL) Setting up nodes and links..."
set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]
set n5 [$ns node]

$n0 shape circle
$n1 shape hexagon
$n2 shape hexagon
$n3 shape hexagon
$n4 shape circle
$n5 shape hexagon

$n0 color red
$n1 color blue
$n2 color blue
$n3 color blue
$n4 color red
$n5 color blue

#################################################
# LINKS
#################################################

$ns duplex-link $n0 $n2 2Mb 2ms DropTail
$ns duplex-link $n1 $n2 2Mb 2ms DropTail

$ns duplex-link $n2 $n3 1.5Mb 10ms DropTail
$ns duplex-link $n3 $n4 1.5Mb 10ms DropTail
$ns queue-limit $n2 $n3 5
$ns duplex-link-op $n2 $n3 queuePos 0


$ns duplex-link $n2 $n5 1.5Mb 10ms DropTail
$ns duplex-link $n5 $n4 1.5Mb 10ms DropTail
$ns queue-limit $n2 $n5 5
$ns duplex-link-op $n2 $n5 queuePos 0
$ns duplex-link-op $n5 $n4 queuePos 0


# Assigning link costs	
$ns duplex-cost $n2 $n5 2
$ns duplex-cost $n5 $n2 2


#################################################
# Configuring TRAFFIC objects
#################################################
puts "(TCL) Configuring traffic objects..."

set tcp1 [new Agent/TCP]
$ns attach-agent $n0 $tcp1
$n0 label "agent TCP"
set snk1 [new Agent/TCPSink]
$ns attach-agent $n4 $snk1
$n4 label "agent TCPSink"
$ns connect $tcp1 $snk1

set cbr1 [new Application/Traffic/CBR]
$cbr1 attach-agent $tcp1
$cbr1 set packetSize_ 60
$cbr1 set interval_ 0.01
$tcp1 set fid_ 1
$ns color 1 magenta


#################################################
# Configuring ROUTING objects
#################################################
puts "(TCL) Configuring routing protocol..."

# set OSPF paremeters
Agent/rtProto/OSPF set helloInterval 1
Agent/rtProto/OSPF set routerDeadInterval 4  

# set OSPF's packets colours
$ns setup-ospf-colors
# Set the routing protocol to OSPF
$ns rtproto OSPF 

#################################################
# Scheduling simulation
#################################################

$ns at 1 "$cbr1 start"
$ns at 2 "$cbr1 stop"
$ns at 2 "finish"


puts "(TCL) Starting simulation..."
puts ""
$ns run

