/* 
 *
 * $Id$
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2004 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#ifndef _K3B_PLUGIN_H_
#define _K3B_PLUGIN_H_

#include <qobject.h>

#define K3B_PLUGIN_SYSTEM_VERSION 2

class K3bPlugin : public QObject
{
  Q_OBJECT

 public:
  K3bPlugin( QObject* parent = 0, const char* name = 0 );
  virtual ~K3bPlugin();
};

#endif
