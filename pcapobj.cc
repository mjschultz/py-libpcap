/*
 * Copyright (c) 2003 CORE Security Technologies
 *
 * This software is provided under under a slightly modified version
 * of the Apache Software License. See the accompanying LICENSE file
 * for more information.
 *
 * $Id: pcapobj.cc,v 1.10 2007/03/27 17:26:13 max Exp $
 */

#include <Python.h>
#include <pcap.h>

#include "pcapobj.h"
#include "pcapy.h"
#include "pcapdumper.h"
#include "pcap_pkthdr.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif


// internal pcapobject
typedef struct {
	PyObject_HEAD
	pcap_t *pcap;
	bpf_u_int32 net;
	bpf_u_int32 mask;
} pcapobject;


// PcapType

static void
pcap_dealloc(register pcapobject* pp)
{
  if ( pp->pcap )
    pcap_close(pp->pcap);

  pp->pcap = NULL;

  PyObject_Del(pp);
}


// pcap methods
static PyObject* p_getnet(register pcapobject* pp, PyObject* args);
static PyObject* p_getmask(register pcapobject* pp, PyObject* args);
static PyObject* p_setfilter( register pcapobject* pp, PyObject* args );
static PyObject* p_next(register pcapobject* pp, PyObject* args);
static PyObject* p_dispatch(register pcapobject* pp, PyObject* args);
static PyObject* p_loop(register pcapobject* pp, PyObject* args);
static PyObject* p_datalink(register pcapobject* pp, PyObject* args);
static PyObject* p_setnonblock(register pcapobject* pp, PyObject* args);
static PyObject* p_getnonblock(register pcapobject* pp, PyObject* args);
static PyObject* p_dump_open(register pcapobject* pp, PyObject* args);
static PyObject* p_sendpacket(register pcapobject* pp, PyObject* args);


static PyMethodDef p_methods[] = {
  {"loop", (PyCFunction) p_loop, METH_VARARGS, "loops packet dispatching"},
  {"dispatch", (PyCFunction) p_dispatch, METH_VARARGS, "dispatchs packets"},
  {"next", (PyCFunction) p_next, METH_VARARGS, "returns next packet"},
  {"setfilter", (PyCFunction) p_setfilter, METH_VARARGS, "compiles and sets a BPF capture filter"},
  {"getnet", (PyCFunction) p_getnet, METH_VARARGS, "returns the network address for the device"},
  {"getmask", (PyCFunction) p_getmask, METH_VARARGS, "returns the netmask for the device"},
  {"datalink", (PyCFunction) p_datalink, METH_VARARGS, "returns the link layer type"},
  {"getnonblock", (PyCFunction) p_getnonblock, METH_VARARGS, "returns the current `non-blocking' state"},
  {"setnonblock", (PyCFunction) p_setnonblock, METH_VARARGS, "puts into `non-blocking' mode, or take it out, depending on the argument"},
  {"dump_open", (PyCFunction) p_dump_open, METH_VARARGS, "creates a dumper object"},
  {"sendpacket", (PyCFunction) p_sendpacket, METH_VARARGS, "sends a packet through the interface"},
  {NULL, NULL}	/* sentinel */
};

static PyObject*
pcap_getattr(pcapobject* pp, char* name)
{
  return Py_FindMethod(p_methods, (PyObject*)pp, name);
}


PyTypeObject Pcaptype = {
  PyObject_HEAD_INIT(NULL)
  0,
  "Reader",
  sizeof(pcapobject),
  0,

  /* methods */
  (destructor)pcap_dealloc,  /*tp_dealloc*/
  0,			  /*tp_print*/
  (getattrfunc)pcap_getattr, /*tp_getattr*/
  0,			  /*tp_setattr*/
  0,			  /*tp_compare*/
  0,			  /*tp_repr*/
  0,			  /*tp_as_number*/
  0,			  /*tp_as_sequence*/
  0,			  /*tp_as_mapping*/
};


PyObject*
new_pcapobject(pcap_t *pcap, bpf_u_int32 net, bpf_u_int32 mask)
{
  pcapobject *pp;

  pp = PyObject_New(pcapobject, &Pcaptype);
  if (pp == NULL)
    return NULL;

  pp->pcap = pcap;
  pp->net = net;
  pp->mask = mask;

  return (PyObject*)pp;
}

static void ntos(char* dst, unsigned int n, int ip)
{
  ip = htonl(ip);
  snprintf(dst, n, "%i.%i.%i.%i",
	   ((ip >> 24) & 0xFF),
	   ((ip >> 16) & 0xFF),
	   ((ip >> 8) & 0xFF),
	   (ip & 0xFF));
}

static PyObject*
p_getnet(register pcapobject* pp, PyObject* args)
{
  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  char ip_str[20];
  ntos(ip_str, sizeof(ip_str), pp->net);
  return Py_BuildValue("s", ip_str);
}

static PyObject*
p_getmask(register pcapobject* pp, PyObject* args)
{
  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  char ip_str[20];
  ntos(ip_str, sizeof(ip_str), pp->mask);
  return Py_BuildValue("s", ip_str);
}

static PyObject*
p_setfilter(register pcapobject* pp, PyObject* args)
{
  struct bpf_program bpfprog;
  int status;
  char* str;

  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
	return NULL;
    }

  if (!PyArg_ParseTuple(args,"s:setfilter",&str))
    return NULL;

  status = pcap_compile(pp->pcap, &bpfprog, str, 1, pp->mask);
  if (status)
    {
      PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
      return NULL;
    }

  status = pcap_setfilter(pp->pcap, &bpfprog);
  if (status)
    {
      PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
      return NULL;
    }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject*
p_next(register pcapobject* pp, PyObject* args)
{
  struct pcap_pkthdr hdr;
  const unsigned char *buf;

  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  // allow threads as this might block
  Py_BEGIN_ALLOW_THREADS;
  buf = pcap_next(pp->pcap, &hdr);
  Py_END_ALLOW_THREADS;

  if(!buf)
    {
      PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
      return NULL;
    }

  PyObject *pkthdr = new_pcap_pkthdr(&hdr);

  return Py_BuildValue("(Os#)", pkthdr, buf, hdr.caplen);
}


struct PcapCallbackContext {
  PcapCallbackContext(pcap_t* p, PyObject* f, PyThreadState* ts)
    : ppcap_t(p), pyfunc(f), thread_state(ts)
  {
    Py_INCREF(pyfunc);
  }
  ~PcapCallbackContext()
  {
    Py_DECREF(pyfunc);
  }

  pcap_t* ppcap_t;
  PyObject *pyfunc;
  PyThreadState *thread_state;

};


static void
PythonCallBack(u_char *user,
	       const struct pcap_pkthdr *header,
	       const u_char *packetdata)
{
  PyObject *arglist, *result;
  unsigned int *len;
  PcapCallbackContext *pctx;
  len    = (unsigned int *)&header->caplen;
  pctx = (PcapCallbackContext *)user;

  PyEval_RestoreThread(pctx->thread_state);

  PyObject *hdr = new_pcap_pkthdr(header);

  arglist = Py_BuildValue("Os#", hdr, packetdata, *len);
  result = PyEval_CallObject(pctx->pyfunc,arglist);

  Py_XDECREF(arglist);
  if (result)
    Py_DECREF(result);

  Py_DECREF(hdr);

  if (!result)
    pcap_breakloop(pctx->ppcap_t);

  PyEval_SaveThread();
}

static PyObject*
p_dispatch(register pcapobject* pp, PyObject* args)
{
  int cant, ret;
  PyObject *PyFunc;

  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  if(!PyArg_ParseTuple(args,"iO:dispatch",&cant,&PyFunc))
    return NULL;

  PcapCallbackContext ctx(pp->pcap, PyFunc, PyThreadState_Get());
  PyEval_SaveThread();
  ret = pcap_dispatch(pp->pcap, cant, PythonCallBack, (u_char*)&ctx);
  PyEval_RestoreThread(ctx.thread_state);

  if(ret<0) {
    if (ret!=-2)  
      /* pcap error, pcap_breakloop was not called so error is not set */
      PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
    return NULL;
  }

  return Py_BuildValue("i", ret);
}

static PyObject*
p_dump_open(register pcapobject* pp, PyObject* args)
{
  char *filename;
  pcap_dumper_t *ret;

  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  if(!PyArg_ParseTuple(args,"s",&filename))
    return NULL;

  ret = pcap_dump_open(pp->pcap, filename);

  if (ret==NULL) {
    PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
    return NULL;
  }

  return new_pcapdumper(ret);
}


static PyObject*
p_loop(register pcapobject* pp, PyObject* args)
{
  int cant, ret;
  PyObject *PyFunc;

  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  if(!PyArg_ParseTuple(args,"iO:loop",&cant,&PyFunc))
    return NULL;

  PcapCallbackContext ctx(pp->pcap, PyFunc, PyThreadState_Get());
  PyEval_SaveThread();
  ret = pcap_loop(pp->pcap, cant, PythonCallBack, (u_char*)&ctx);
  PyEval_RestoreThread(ctx.thread_state);

  if(ret<0) {
    if (ret!=-2)  
      /* pcap error, pcap_breakloop was not called so error is not set */
      PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
    return NULL;
  }

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject*
p_datalink(register pcapobject* pp, PyObject* args)
{
	if (pp->ob_type != &Pcaptype) {
		PyErr_SetString(PcapError, "Not a pcap object");
		return NULL;
	}

	int type = pcap_datalink(pp->pcap);

	return Py_BuildValue("i", type);
}

static PyObject*
p_setnonblock(register pcapobject* pp, PyObject* args)
{
	if (pp->ob_type != &Pcaptype) {
		PyErr_SetString(PcapError, "Not a pcap object");
		return NULL;
	}

	int state;

	if (!PyArg_ParseTuple(args, "i", &state))
		return NULL;

	char errbuf[PCAP_ERRBUF_SIZE];
	int ret = pcap_setnonblock(pp->pcap, state, errbuf);
	if (-1 == ret) {
		PyErr_SetString(PcapError, errbuf);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject*
p_getnonblock(register pcapobject* pp, PyObject* args)
{
	if (pp->ob_type != &Pcaptype) {
		PyErr_SetString(PcapError, "Not a pcap object");
		return NULL;
	}

	char errbuf[PCAP_ERRBUF_SIZE];
	int state = pcap_getnonblock(pp->pcap, errbuf);
	if (-1 == state) {
		PyErr_SetString(PcapError, errbuf);
		return NULL;
	}

	return Py_BuildValue("i", state);
}

static PyObject*
p_sendpacket(register pcapobject* pp, PyObject* args)
{
  int status;
  unsigned char* str;
  unsigned int length;

  if (pp->ob_type != &Pcaptype)
    {
      PyErr_SetString(PcapError, "Not a pcap object");
      return NULL;
    }

  if (!PyArg_ParseTuple(args,"s#", &str, &length))
    return NULL;

  status = pcap_sendpacket(pp->pcap, str, length);
  if (status)
    {
      PyErr_SetString(PcapError, pcap_geterr(pp->pcap));
      return NULL;
    }

  Py_INCREF(Py_None);
  return Py_None;
}
