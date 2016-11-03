
# AUTORA: Inés María Romero Dávia

# OBJETIVO
#   	PRTOCOLO DE ENCAMINAMIENTO OSPF
#
# 	Escenario para probar el protocolo de routing OSPF sobre network simulator .
# 	No se configura el protocolo OSPF en todos los nodos de la topología
#	Las conexiones lógicas son:
#
#			n0 ->n4
#			n1 ->n4
# TOPOLOGIA
#
#		$n0       $n3 _ _ $n6  
#		   \     /   	      \ 
#		    \   /     	       \	
#		     $n2	       $n4
#		    /	\              / 
#		   /     \            /  	 
#		$n1       \          / 
#			      $n5	
#
#       7 nodos 0 - 6
#       Links entre 0-2, 1-2
#               bandwith 10 Mbps    delay 2ms      DropTail
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
#       Agente TCP
#               Nodo 1
#       Agente null/Sink
#               Nodo 4
#       Un agente Traffic CBR para generar el trafico
#               Nodo 1 Packetsize 60    Interval 0.02      
#

# AGENTES DE ROUTING
#	Agente OSPF
#		Nodos: 0,1,2,3,4,6
#
# COSTE ENLACES:
# Topologia mtid 0
#	Todos = 1
# Topologia mtid 1
#	Todos = 2
# Topologia mtid 2
#	Todos = 3

# MULTIPATH
#	Activado	
#
# MTROUTING
#	Activado	
#
# IDENTIFICADOR MULTITOPOLOGIA
#	MtId = 0	
#
# NUMERO DE IDENTIFICADORES MULTITOPOLOGIAS
#	Agente TCP 0: mtid = 0	
#	Agente TCP 1: mtid = 1

######################################################################################


Agent/TCP set syn_ false
Agent/TCP set delay_growth_ false
# In preparation for changing the default values for syn_ and delay_growth_.

remove-all-packet-headers       ; # removes all except common
add-packet-header Flags IP TCP rtProtoOSPF ; # hdrs reqd for validation test


puts "(TCL) Creating simulator & trace files..."
set dir "./out_ospf9"

set ns [new Simulator]
set vj_ss true

set f [eval open $dir/ospf9.tr w]
set nf [open $dir/ospf9.nam w]
$ns trace-all $f
$ns namtrace-all $nf

proc finish {} {
	global ns f nf dir
	$ns flush-trace
	close $f
	close $nf
	eval exec nam $dir/ospf9.nam 
  puts "(TCL) Finishing..."
	exit 0
}

# Enable multiPath routing
Node set multiPath_ 1
# Enable mtrouting
Node set mtRouting_ 1


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
set n6 [$ns node]

$n0 shape circle
$n1 shape circle
$n2 shape hexagon
$n3 shape hexagon
$n4 shape circle
$n5 shape hexagon
$n6 shape hexagon

$n0 color red
$n1 color red
$n2 color blue
$n3 color blue
$n4 color red
$n5 color blue
$n6 color blue

#################################################
# LINKS
#################################################

$ns duplex-link $n0 $n2 10Mb 2ms DropTail
$ns duplex-link $n1 $n2 10Mb 2ms DropTail

$ns duplex-link $n2 $n3 1.5Mb 10ms DropTail
$ns duplex-link $n3 $n6 1.5Mb 10ms DropTail
$ns duplex-link $n6 $n4 1.5Mb 10ms DropTail
$ns queue-limit $n2 $n3 5
$ns duplex-link-op $n2 $n3 queuePos 0


$ns duplex-link $n2 $n5 1.5Mb 10ms DropTail
$ns duplex-link $n5 $n4 1.5Mb 10ms DropTail
$ns queue-limit $n2 $n5 5
$ns duplex-link-op $n2 $n5 queuePos 0
$ns duplex-link-op $n5 $n4 queuePos 0

#set initial costs 
$ns init-links-cost

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
$cbr1 set interval_ 0.02
$tcp1 set fid_ 1
$ns color 1 magenta


set tcp2 [new Agent/TCP]
$ns attach-agent $n1 $tcp2
$n1 label "agent TCP"
set snk2 [new Agent/TCPSink]
$ns attach-agent $n4 $snk2
$ns connect $tcp2 $snk2

set cbr2 [new Application/Traffic/CBR]
$cbr2 attach-agent $tcp2
$cbr2 set packetSize_ 60
$cbr2 set interval_ 0.02
$tcp2 set fid_ 2
$ns color 2 cyan

# configure mtid in packets
$ns configure-mtid $tcp1 0
$ns configure-mtid $snk1 0
$ns configure-mtid $tcp2 1
$ns configure-mtid $snk2 1

#################################################
# Configuring ROUTING objects
#################################################
puts "(TCL) Configuring routing protocol..."

# set OSPF paremeters
Agent/rtProto/OSPF set helloInterval 1
Agent/rtProto/OSPF set routerDeadInterval 4 


# set number of MT ids
Simulator  set numMtIds 2

# set OSPF's packets colours
$ns setup-ospf-colors
# Set the routing protocol to OSPF
$ns rtproto OSPF [list $n0 $n1 $n2 $n3 $n4 $n6]

#################################################
# Scheduling simulation
#################################################
puts "(TCL) Configuring routing protocol..."

$ns at 0.8 "$cbr1 start"
$ns at 0.8 "$cbr2 start"
$ns at 20 "$cbr1 stop"
$ns at 20 "$cbr2 stop"
$ns at 20 "finish"


puts "(TCL) Starting simulation..."
puts ""
$ns run

