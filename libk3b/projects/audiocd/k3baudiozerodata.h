/* 
 *
 * $Id$
 * Copyright (C) 2004 Sebastian Trueg <trueg@k3b.org>
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

#ifndef _K3B_AUDIO_ZERO_DATA_H_
#define _K3B_AUDIO_ZERO_DATA_H_

#include "k3baudiodatasource.h"

class K3bAudioZeroData : public K3bAudioDataSource
{
 public:
  K3bAudioZeroData( K3bAudioDoc* doc, const K3b::Msf& msf = 150 );
  ~K3bAudioZeroData();

  K3b::Msf length() const { return m_length; }
  void setLength( const K3b::Msf& msf );

  QString type() const;
  QString sourceComment() const;

  bool seek( const K3b::Msf& );
  int read( char* data, unsigned int max );

  K3bAudioDataSource* copy() const;

  K3bAudioDataSource* split( const K3b::Msf& pos );

 private:
  K3b::Msf m_length;
  unsigned long long m_writtenData;
};

#endif
