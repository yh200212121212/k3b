/* 
 *
 * $Id$
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef _K3B_CUEFILE_PARSER_H_
#define _K3B_CUEFILE_PARSER_H_

#include "k3bimagefilereader.h"


/**
 * This class should be able to parse all cuefile stuff.
 * for now it only checks the ending and searches for a file
 * statement.
 */
class K3bCueFileParser : public K3bImageFileReader
{
 public:
  K3bCueFileParser( const QString& filename = QString::null );
  ~K3bCueFileParser();

 private:
  void readFile();
};

#endif
