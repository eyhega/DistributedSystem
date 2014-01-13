/**
 * \file entete.c
 * \author Antoine DUFAURE & Gaetan EYHERAMONO
 * */
#include "entete.h"

int xdr_couple(XDR *xdrp, struct couple *p)
{
	return(xdr_vector(xdrp,p->_name,BUFF_SIZE,sizeof(char),(xdrproc_t)xdr_char)
	 && xdr_u_short(xdrp,&p->_port));
}
