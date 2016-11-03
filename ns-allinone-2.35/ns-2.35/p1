/*
 * classifier-mtopology.cc
 * Copyright (C) 2006 by the University of Extremadura
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. This program is distributed in the 
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR 
 * A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details. You should have received a copy of the GNU General 
 * Public License along with this program. If not, see <http://www.gnu.org/licenses/>. 
 */

// by Inés María Romero Dávila (inroda04@alumnos.unex.es)
// and Alfonso Gazo Cervero (agazo@unex.es)

/***********************************************************************************
 Módulo en el que se crea un nuevo tipo de clasificador que soporta el multitopology
 routing. Permite direccionar los paquetes en función de su identificador
 multitopología.
************************************************************************************/

#include "classifier-mtopology.h"

int  MTopoClassifier::classify(Packet *p) {
	hdr_ip* iph = hdr_ip::access(p);
	//printf("\nclassifier mtid: %d",iph->mtid());	
	return (iph->mtid());
};

