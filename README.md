# ns-hack

##Breif installation procedure
change linkstate/ls.h -> line 137 >> ADD this->
`./install

cd ns-2.35/

sudo make install

cp ns ns-ospf

sudo cp ns-ospf /usr/local/bin/

cd tcl/ex/ospf/

mkdir out_ospf0 out_ospf1 out_ospf2 out_ospf3 out_ospf4 out_ospf5 out_ospf6 out_ospf7 out_ospf8 out_ospf9

ns-ospf ospf0.tcl`
