/*
 * Copyright (c) 2003 CORE Security Technologies
 *
 * This software is provided under under a slightly modified version
 * of the Apache Software License. See the accompanying LICENSE file
 * for more information.
 *
 * $Id: bpfobj.h,v 1.3 2003/10/24 18:49:33 jkohen Exp $
 */

#ifndef __bpfobj__
#define __bpfobj__

PyObject*
new_bpfobject(const struct bpf_program &bpf);

extern PyTypeObject Pcaptype;

#endif // __bpfobj__
