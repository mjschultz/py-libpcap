/*
 * Copyright (c) 2003 CORE Security Technologies
 *
 * This software is provided under under a slightly modified version
 * of the Apache Software License. See the accompanying LICENSE file
 * for more information.
 *
 * $Id: pcapy.cc,v 1.7 2005/12/09 15:07:04 max Exp $
 */

#include <pcap.h>
#include <Python.h>

#include "pcapy.h"
#include "pcapobj.h"
#include "bpfobj.h"


PyObject *PcapError;



// module methods

static PyObject* 
lookupdev(PyObject* self, PyObject* args)
{
  char errbuff[PCAP_ERRBUF_SIZE];
  char* dev;
  
  dev = pcap_lookupdev(errbuff);
  if(!dev)
    {
      PyErr_SetString(PcapError, errbuff);
      return NULL;
    }
  
  return Py_BuildValue("u", dev);
}

static PyObject*
findalldevs(PyObject *self, PyObject *args)
{
  char errbuff[PCAP_ERRBUF_SIZE];
  pcap_if_t *devs;

  int status = pcap_findalldevs(&devs, errbuff);
  if(status)
    {
      PyErr_SetString(PcapError, errbuff);
      return NULL;
    }

  if(devs==NULL)
    {
      PyErr_SetString(PcapError, "No valid interfaces to open");
      return NULL;
    }

  pcap_if_t *cursor = devs;
  PyObject* list = PyList_New(0);
  while(cursor)
    {
      PyList_Append(list, Py_BuildValue("s", cursor->name));
      cursor = cursor->next;
    }

  pcap_freealldevs(devs);
  
  return list;
}

static PyObject*
open_live(PyObject *self, PyObject *args)
{
  char errbuff[PCAP_ERRBUF_SIZE];
  char * device;
  int  snaplen;
  int  promisc;
  int  to_ms;
  
  bpf_u_int32 net, mask;
  
  
  if(!PyArg_ParseTuple(args,"siii:open_live",&device,&snaplen,&promisc,&to_ms))
    return NULL;
  
  int status = pcap_lookupnet(device, &net, &mask, errbuff);
  if(status)
    {
      net = 0;
      mask = 0;
    }
  
  pcap_t* pt;
	
  pt = pcap_open_live(device, snaplen, promisc!=0, to_ms, errbuff);
  if(!pt)
    {
      PyErr_SetString(PcapError, errbuff);
      return NULL;
    }
#ifdef WIN32
  pcap_setmintocopy(pt, 0);
#endif

  return new_pcapobject( pt, net, mask );
}

static PyObject*
open_offline(PyObject *self, PyObject *args)
{
  char errbuff[PCAP_ERRBUF_SIZE];
  char * filename;
  
  
  if(!PyArg_ParseTuple(args,"s",&filename))
    return NULL;
  
  pcap_t* pt;
	
  pt = pcap_open_offline(filename, errbuff);
  if(!pt)
    {
      PyErr_SetString(PcapError, errbuff);
      return NULL;
    }
#ifdef WIN32
  pcap_setmintocopy(pt, 0);
#endif

  return new_pcapobject( pt );
}


static PyObject*
bpf_compile(PyObject* self, PyObject* args)
{
  int linktype;
  int  snaplen;
  char *filter;
  int optimize;
  int netmask;
  
  if(!PyArg_ParseTuple(args, 
		       "iisii:compile",
		       &linktype,
		       &snaplen,
		       &filter,
		       &optimize,
		       &netmask))
    return NULL;
  
  pcap_t *pp;

  pp = pcap_open_dead(linktype, snaplen);
  if(pp == NULL)
    return NULL;

  struct bpf_program bpf;
  int status = pcap_compile(pp, &bpf, filter, optimize, netmask);
  pcap_close(pp);

  if(status)
    {
      PyErr_SetString(PcapError, pcap_geterr(pp));
      return NULL;
    }
  
  return new_bpfobject( bpf );
}


static PyMethodDef pcap_methods[] = {
  {"open_live", open_live, METH_VARARGS, "open_live(device, snaplen, promisc, to_ms) opens a pcap device"},
  {"open_offline", open_offline, METH_VARARGS, "open_offline(filename) opens a pcap formated file"},
  {"lookupdev", lookupdev, METH_VARARGS, "lookupdev() looks up a pcap device"},
  {"findalldevs", findalldevs, METH_VARARGS, "findalldevs() lists all available interfaces"},
  {"compile", bpf_compile, METH_VARARGS, "compile(linktype, snaplen, filter, optimize, netmask) creates a bpfprogram object"},
  {NULL, NULL}
};


static char *pcap_doc =
"\nA wrapper for the Packet Capture (PCAP) library\n";


void
initpcapy(void)
{
  PyObject *m, *d;

  Pcaptype.ob_type =  &PyType_Type;
  m = Py_InitModule3("pcapy", pcap_methods, pcap_doc);

  /* Direct from pcap's net/bpf.h. */
  PyModule_AddIntConstant(m, "DLT_NULL", 0);
  PyModule_AddIntConstant(m, "DLT_EN10MB", 1);
  PyModule_AddIntConstant(m, "DLT_IEEE802", 6);
  PyModule_AddIntConstant(m, "DLT_ARCNET", 7);
  PyModule_AddIntConstant(m, "DLT_SLIP", 8);
  PyModule_AddIntConstant(m, "DLT_PPP", 9);
  PyModule_AddIntConstant(m, "DLT_FDDI", 10);
  PyModule_AddIntConstant(m, "DLT_ATM_RFC1483", 11);
  PyModule_AddIntConstant(m, "DLT_RAW", 12);
  PyModule_AddIntConstant(m, "DLT_PPP_SERIAL", 50);
  PyModule_AddIntConstant(m, "DLT_PPP_ETHER", 51);
  PyModule_AddIntConstant(m, "DLT_C_HDLC", 104);
  PyModule_AddIntConstant(m, "DLT_IEEE802_11", 105);
  PyModule_AddIntConstant(m, "DLT_LOOP", 108);
  PyModule_AddIntConstant(m, "DLT_LINUX_SLL", 113);
  PyModule_AddIntConstant(m, "DLT_LTALK", 114);

  d = PyModule_GetDict(m);
  PcapError = PyErr_NewException("pcapy.PcapError", NULL, NULL );
  if( PcapError )
    PyDict_SetItemString( d, "PcapError", PcapError );
}
