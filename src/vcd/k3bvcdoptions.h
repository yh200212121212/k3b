/*
 *
 * $Id$
 * Copyright (C) 2003 Christian Kvasny <chris@k3b.org>
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

#ifndef K3B_VCD_OPTIONS_H
#define K3B_VCD_OPTIONS_H

#include <qstring.h>

class KConfig;


class K3bVcdOptions
{
 public:
  K3bVcdOptions();

  const QString& volumeId() const { return m_volumeID; }
  const QString& albumId() const { return m_albumID; }
  const QString& volumeSetId() const { return m_volumeSetId; }
  const QString& preparer() const { return m_preparer; }
  const QString& publisher() const { return m_publisher; }

  const QString& applicationId() const { return m_applicationId; }
  const QString& systemId() const { return m_systemId; }

  const QString& vcdClass() const { return m_vcdclass; }
  const QString& vcdVersion() const { return m_vcdversion; }

  const int volumeCount() const { return m_volumeCount; }
  const int volumeNumber()const { return m_volumeNumber; }

  const bool AutoDetect() const { return m_autodetect; }
  const bool CdiSupport() const { return m_cdisupport; }
  const bool BrokenSVcdMode() const { return m_brokensvcdmode; }
  const bool Sector2336() const { return m_sector2336; }
  const unsigned long long CDIsize() const {return m_cdisize;}

  void setAlbumId( const QString& s ) { m_albumID = s; }
  void setVolumeId( const QString& s ) { m_volumeID = s; }
  void setVolumeSetId( const QString& s ) { m_volumeSetId = s; }
  void setPreparer( const QString& s ) { m_preparer = s; }
  void setPublisher( const QString& s ) { m_publisher = s; }

  void setVcdClass( const QString& s ) { m_vcdclass = s; }
  void setVcdVersion( const QString& s ) { m_vcdversion = s; }

  void setVolumeCount( const int c ) { m_volumeCount = c; }
  void setVolumeNumber( const int n ) { m_volumeNumber = n; }
  
  void setAutoDetect( const bool& b ) { m_autodetect = b; }
  void setCdiSupport( const bool& b ) { m_cdisupport = b; }
  void setBrokenSVcdMode( const bool& b ) { m_brokensvcdmode = b; }
  void setSector2336( const bool& b ) { m_sector2336 = b; }
  
  bool checkCdiFiles();
  void save( KConfig* c );

  static K3bVcdOptions load( KConfig* c );
  static K3bVcdOptions defaults();

 private:
  // volume descriptor
  QString m_volumeID;
  QString m_albumID;
  QString m_volumeSetId;

  QString m_preparer;
  QString m_publisher;

  QString m_applicationId;
  QString m_systemId;

  QString m_vcdclass;
  QString m_vcdversion;
    
  int m_volumeCount;
  int m_volumeNumber;
    
  bool m_autodetect;
  bool m_cdisupport;
  bool m_brokensvcdmode;
  bool m_sector2336;

  unsigned long long m_cdisize;
};

#endif
