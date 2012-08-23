/*
 * Copyright (c) 2003 CORE Security Technologies
 *
 * This software is provided under under a slightly modified version
 * of the Apache Software License. See the accompanying LICENSE file
 * for more information.
 *
 * $Id: pcapy.h,v 1.2 2003/10/23 20:00:53 jkohen Exp $
 */

#ifndef __PCAPY_H__


extern "C" {
#ifdef WIN32
__declspec(dllexport)
#endif
void initpcapy(void);
}

// exception object
extern PyObject* PcapError;

#endif // __PCAPY_H__
