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


#ifndef _K3B_DVD_JOB_H_
#define _K3B_DVD_JOB_H_

#include <k3bjob.h>

#include <qfile.h>

class K3bDataDoc;
class K3bGrowisofsImager;
class K3bGrowisofsWriter;
class K3bIsoImager;
class QDataStream;


class K3bDvdJob : public K3bBurnJob
{
  Q_OBJECT

 public:
  /**
   * To be more flexible we allow writing of any data doc
   */
  K3bDvdJob( K3bDataDoc*, QObject* parent = 0 );
  virtual ~K3bDvdJob();

  K3bDoc* doc() const;
  K3bCdDevice::CdDevice* writer() const;

  virtual QString jobDescription() const;
  virtual QString jobDetails() const;

 public slots:
  void start();
  void cancel();

 protected:
  virtual bool prepareWriterJob();
  void prepareIsoImager();
  void prepareGrowisofsImager();
  void cleanup();
  void writeImage();

  /**
   * Only used by the VideoDvdJob
   */
  void setVideoDvd( bool b ) { m_videoDvd = b; }

 protected slots:
  void slotReceivedIsoImagerData( const char* data, int len );
  void slotIsoImagerFinished( bool success );
  void slotIsoImagerPercent(int);
  void slotGrowisofsImagerPercent(int);

  void slotWriterJobPercent( int );
  void slotWritingFinished( bool );

  void slotVerificationProgress( int p );
  void slotVerificationFinished( bool success );

 private:
  K3bDataDoc* m_doc;
  K3bIsoImager* m_isoImager;
  K3bGrowisofsImager* m_growisofsImager;
  K3bGrowisofsWriter* m_writerJob;

  QFile m_imageFile;
  QDataStream m_imageFileStream;

  bool m_canceled;
  bool m_writingStarted;

  bool m_videoDvd;

  bool waitForDvd();

  class Private;
  Private* d;
};

#endif
