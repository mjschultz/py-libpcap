/*
 * Copyright (c) 2003 CORE Security Technologies
 *
 * This software is provided under under a slightly modified version
 * of the Apache Software License. See the accompanying LICENSE file
 * for more information.
 *
 * $Id: pcapdumper.h,v 1.2 2003/10/23 20:00:53 jkohen Exp $
 */

#ifndef __pcapdumper__
#define __pcapdumper__


PyObject*
new_pcapdumper(pcap_dumper_t *dumper);

extern PyTypeObject Pdumpertype;

#endif // __pcapdumper__
