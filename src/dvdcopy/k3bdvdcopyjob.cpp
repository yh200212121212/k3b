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

#include "k3bdvdcopyjob.h"

#include <k3breadcdreader.h>
#include <k3bdatatrackreader.h>
#include <k3bexternalbinmanager.h>
#include <k3bdevice.h>
#include <k3bdevicehandler.h>
#include <k3bdiskinfo.h>
#include <k3bglobals.h>
#include <k3bcore.h>
#include <k3bgrowisofswriter.h>
#include <k3breadcdreader.h>
#include <k3bversion.h>
#include <k3biso9660.h>

#include <kdebug.h>
#include <klocale.h>
#include <kio/global.h>
#include <kmessagebox.h>

#include <qfile.h>
#include <qapplication.h>


class K3bDvdCopyJob::Private
{
public:
  Private() 
    : doneCopies(0),
      running(false),
      canceled(false),
      writerJob(0),
      readcdReader(0),
      dataTrackReader(0),
      usedWritingMode(0) {
  }

  int doneCopies;

  bool running;
  bool readerRunning;
  bool writerRunning;
  bool canceled;

  K3bGrowisofsWriter* writerJob;
  K3bReadcdReader* readcdReader;
  K3bDataTrackReader* dataTrackReader;

  //  QFile bufferFile;

  K3b::Msf lastSector;

  int usedWritingMode;
};


K3bDvdCopyJob::K3bDvdCopyJob( K3bJobHandler* hdl, QObject* parent, const char* name )
  : K3bBurnJob( hdl, parent, name ),
    m_writerDevice(0),
    m_readerDevice(0),
    m_onTheFly(false),
    m_removeImageFiles(false),
    m_simulate(false),
    m_speed(1),
    m_copies(1),
    m_onlyCreateImage(false),
    m_ignoreReadErrors(false),
    m_readRetries(128),
    m_writingMode( K3b::WRITING_MODE_AUTO )
{
  d = new Private();
}


K3bDvdCopyJob::~K3bDvdCopyJob()
{
  delete d;
}


void K3bDvdCopyJob::start()
{
  emit started();
  emit burning(false);

  d->canceled = false;
  d->running = true;
  d->readerRunning = d->writerRunning = false;

  if( m_onTheFly && 
      k3bcore->externalBinManager()->binObject( "growisofs" )->version < K3bVersion( 5, 12 ) ) {
    m_onTheFly = false;
    emit infoMessage( i18n("K3b does not support writing on-the-fly with growisofs %1.")
		      .arg(k3bcore->externalBinManager()->binObject( "growisofs" )->version), ERROR );
    emit infoMessage( i18n("Disabling on-the-fly writing."), INFO );
  }

  emit infoMessage( i18n("Checking source media") + "...", INFO );
  emit newSubTask( i18n("Checking source media") );

  connect( K3bCdDevice::sendCommand( K3bCdDevice::DeviceHandler::DISKINFO, m_readerDevice ),
           SIGNAL(finished(K3bCdDevice::DeviceHandler*)),
           this,
           SLOT(slotDiskInfoReady(K3bCdDevice::DeviceHandler*)) );
}


void K3bDvdCopyJob::slotDiskInfoReady( K3bCdDevice::DeviceHandler* dh )
{
  if( d->canceled ) {
    emit canceled();
    emit finished(false);
    d->running = false;
  }

  if( dh->ngDiskInfo().empty() || dh->ngDiskInfo().diskState() == K3bCdDevice::STATE_NO_MEDIA ) {
    emit infoMessage( i18n("No source media found."), ERROR );
    emit finished(false);
    d->running = false;
  }
  else {
    if( dh->ngDiskInfo().capacity() > K3b::Msf( 510*60*75 ) ) {
      kdDebug() << "(K3bDvdCopyJob) DVD too large." << endl;

      if( KMessageBox::warningYesNo( qApp->activeWindow(), 
				     i18n("The source DVD seems to be too large (%1) to make its contents fit on "
					  "a normal writable DVD media which have a capacity of approximately "
					  "4.3 Gigabytes. Do you really want to continue?")
				     .arg(KIO::convertSize( dh->ngDiskInfo().capacity().mode1Bytes() ) ),
				     i18n("Source DVD too large") ) == KMessageBox::No ) {
	emit canceled();
	emit finished(false);
	d->running = false;
	return;
      }
    }

    //
    // We cannot rely on the kernel to determine the size of the DVD for some reason
    // On the other hand it is not always a good idea to rely on the size from the ISO9660
    // header since that may be wrong due to some buggy encoder or some boot code appended
    // after creating the image.
    // That is why we try our best to determine the size of the DVD. For DVD-ROM this is very
    // easy since it has only one track. The same goes for single session DVD-R(W) and DVD+R.
    // Multisession DVDs we will simply not copy. ;)
    // For DVD+RW and DVD-RW in restricted overwrite mode we are left with no other choice but
    // to use the ISO9660 header.
    //
    // On the other hand: in on-the-fly mode growisofs determines the size of the data to be written
    //                    by looking at the ISO9660 header when writing in DAO mode. So in this case
    //                    it would be best for us to do the same....
    //
    // With growisofs 5.15 we have the option to specify the size of the image to be written in DAO mode.
    //

    switch( dh->ngDiskInfo().mediaType() ) {
    case K3bCdDevice::MEDIA_DVD_ROM:
    case K3bCdDevice::MEDIA_DVD_R:
    case K3bCdDevice::MEDIA_DVD_R_SEQ:
    case K3bCdDevice::MEDIA_DVD_RW:
    case K3bCdDevice::MEDIA_DVD_RW_SEQ:
    case K3bCdDevice::MEDIA_DVD_PLUS_R:

      if( dh->ngDiskInfo().numSessions() > 1 ) {
	emit infoMessage( i18n("K3b does not support copying multi-session DVDs."), ERROR );
	emit finished(false);
	d->running = false;
	return;
      }

      // growisofs only uses the size from the PVD for reserving
      // writable space in DAO mode
      // with version >= 5.15 growisofs supports specifying the size of the track
      if( m_writingMode != K3b::DAO || !m_onTheFly || 
	  ( k3bcore->externalBinManager()->binObject( "growisofs" ) && 
	    k3bcore->externalBinManager()->binObject( "growisofs" )->version >= K3bVersion( 5, 15, -1 ) ) ) {
	d->lastSector = dh->toc()[0].lastSector();
	break;
      }

      // fallthrough

    case K3bCdDevice::MEDIA_DVD_PLUS_RW:
    case K3bCdDevice::MEDIA_DVD_RW_OVWR:
      {
	emit infoMessage( i18n("K3b relies on the size saved in the ISO9660 header."), WARNING );
	emit infoMessage( i18n("This might result in a corrupt copy if the source was mastered with buggy software."), WARNING );

	K3bIso9660 isoF( m_readerDevice, 0 );
	if( isoF.open() ) {
	  d->lastSector = ((long long)isoF.primaryDescriptor().logicalBlockSize*isoF.primaryDescriptor().volumeSpaceSize)/2048LL;
	}
	else {
	  emit infoMessage( i18n("Unable to determine the ISO9660 filesystem size."), ERROR );
	  emit finished(false);
	  d->running = false;
	  return;
	}
      }
      break;

    case K3bCdDevice::MEDIA_DVD_RAM:
      emit infoMessage( i18n("K3b does not support copying DVD-RAM."), ERROR );
      emit finished(false);
      d->running = false;
      return;

    default:
      emit infoMessage( i18n("Unable to determine DVD media type."), ERROR );
      emit finished(false);
      d->running = false;
      return;
    }


    if( !m_onTheFly ) {
      //
      // check free temp space
      //
      KIO::filesize_t imageSpaceNeeded = (KIO::filesize_t)(d->lastSector.lba()+1)*2048;
      unsigned long avail, size;
      QString pathToTest = m_imagePath.left( m_imagePath.findRev( '/' ) );
      if( !K3b::kbFreeOnFs( pathToTest, size, avail ) ) {
	emit infoMessage( i18n("Unable to determine free space in temporary directory '%1'.").arg(pathToTest), ERROR );
	emit finished(false);
	d->running = false;
	return;
      }
      else {
	if( avail < imageSpaceNeeded/1024 ) {
	  emit infoMessage( i18n("Not enough space left in temporary directory."), ERROR );
	  emit finished(false);
	  d->running = false;
	  return;
	}
      }
    }


    if( m_onlyCreateImage || !m_onTheFly ) {
      emit newTask( i18n("Creating DVD image") );
    }
    else if( m_onTheFly && !m_onlyCreateImage ) {
      if( waitForDvd() ) {
	prepareWriter();
	if( m_simulate )
	  emit newTask( i18n("Simulating DVD copy") );
	else if( m_copies > 1 )
	  emit newTask( i18n("Writing DVD copy %1").arg(d->doneCopies+1) );
	else
	  emit newTask( i18n("Writing DVD copy") );

	emit burning(true);
	d->writerRunning = true;
	d->writerJob->start();
      }
      else {
	emit finished(false);
	d->running = false;
	return;
      }
    }

    prepareReader();
    d->readerRunning = true;
    d->dataTrackReader->start();
  }
}


void K3bDvdCopyJob::cancel()
{
  if( d->running ) {
    d->canceled = true;
    if( d->readerRunning  )
      d->dataTrackReader->cancel();
    if( d->writerRunning )
      d->writerJob->cancel();
  }
  else {
    kdDebug() << "(K3bDvdCopyJob) not running." << endl;
  }
}


void K3bDvdCopyJob::prepareReader()
{
//   if( !d->readcdReader ) {
//     d->readcdReader = new K3bReadcdReader( this );
//     connect( d->readcdReader, SIGNAL(percent(int)), this, SLOT(slotReaderProgress(int)) );
//     connect( d->readcdReader, SIGNAL(processedSize(int, int)), this, SLOT(slotReaderProcessedSize(int, int)) );
//     connect( d->readcdReader, SIGNAL(finished(bool)), this, SLOT(slotReaderFinished(bool)) );
//     connect( d->readcdReader, SIGNAL(infoMessage(const QString&, int)), this, SIGNAL(infoMessage(const QString&, int)) );
//     connect( d->readcdReader, SIGNAL(newTask(const QString&)), this, SIGNAL(newSubTask(const QString&)) );
//     connect( d->readcdReader, SIGNAL(debuggingOutput(const QString&, const QString&)), 
//              this, SIGNAL(debuggingOutput(const QString&, const QString&)) );
//   }

//   d->readcdReader->setReadDevice( m_readerDevice );
//   d->readcdReader->setReadSpeed( 0 ); // MAX
//   d->readcdReader->setSectorRange( 0, d->lastSector );

//   if( m_onTheFly && !m_onlyCreateImage )
//     d->readcdReader->writeToFd( d->writerJob->fd() );
//   else {
//     d->readcdReader->writeToFd( -1 );
//     d->readcdReader->setImagePath( m_imagePath );
//   }

  if( !d->dataTrackReader ) {
    d->dataTrackReader = new K3bDataTrackReader( this );
    connect( d->dataTrackReader, SIGNAL(percent(int)), this, SLOT(slotReaderProgress(int)) );
    connect( d->dataTrackReader, SIGNAL(processedSize(int, int)), this, SLOT(slotReaderProcessedSize(int, int)) );
    connect( d->dataTrackReader, SIGNAL(finished(bool)), this, SLOT(slotReaderFinished(bool)) );
    connect( d->dataTrackReader, SIGNAL(infoMessage(const QString&, int)), this, SIGNAL(infoMessage(const QString&, int)) );
    connect( d->dataTrackReader, SIGNAL(newTask(const QString&)), this, SIGNAL(newSubTask(const QString&)) );
    connect( d->dataTrackReader, SIGNAL(debuggingOutput(const QString&, const QString&)), 
             this, SIGNAL(debuggingOutput(const QString&, const QString&)) );
  }

  d->dataTrackReader->setDevice( m_readerDevice );
  d->dataTrackReader->setIgnoreErrors( m_ignoreReadErrors );
  d->dataTrackReader->setRetries( m_readRetries );
  d->dataTrackReader->setSectorRange( 0, d->lastSector );
  if( m_onTheFly && !m_onlyCreateImage )
    d->dataTrackReader->writeToFd( d->writerJob->fd() );
  else {
    d->dataTrackReader->writeToFd( -1 );
    d->dataTrackReader->setImagePath( m_imagePath );
  }
}


// ALWAYS CALL WAITFORDVD BEFORE PREPAREWRITER!
void K3bDvdCopyJob::prepareWriter()
{
  delete d->writerJob;

  d->writerJob = new K3bGrowisofsWriter( m_writerDevice, this );

  connect( d->writerJob, SIGNAL(infoMessage(const QString&, int)), this, SIGNAL(infoMessage(const QString&, int)) );
  connect( d->writerJob, SIGNAL(percent(int)), this, SLOT(slotWriterProgress(int)) );
  connect( d->writerJob, SIGNAL(processedSize(int, int)), this, SIGNAL(processedSize(int, int)) );
  connect( d->writerJob, SIGNAL(processedSubSize(int, int)), this, SIGNAL(processedSubSize(int, int)) );
  connect( d->writerJob, SIGNAL(buffer(int)), this, SIGNAL(bufferStatus(int)) );
  connect( d->writerJob, SIGNAL(writeSpeed(int, int)), this, SIGNAL(writeSpeed(int, int)) );
  connect( d->writerJob, SIGNAL(finished(bool)), this, SLOT(slotWriterFinished(bool)) );
  //  connect( d->writerJob, SIGNAL(newTask(const QString&)), this, SIGNAL(newTask(const QString&)) );
  connect( d->writerJob, SIGNAL(newSubTask(const QString&)), this, SIGNAL(newSubTask(const QString&)) );
  connect( d->writerJob, SIGNAL(debuggingOutput(const QString&, const QString&)), 
	   this, SIGNAL(debuggingOutput(const QString&, const QString&)) );

  // these do only make sense with DVD-R(W)
  d->writerJob->setSimulate( m_simulate );
  d->writerJob->setBurnSpeed( m_speed );
  d->writerJob->setWritingMode( d->usedWritingMode );
 
  // this is only used in DAO mode with growisofs >= 5.15
  d->writerJob->setTrackSize( d->lastSector.lba()+1 );
 
  if( m_onTheFly )
    d->writerJob->setImageToWrite( QString::null ); // write to stdin
  else
    d->writerJob->setImageToWrite( m_imagePath );
}


void K3bDvdCopyJob::slotReaderProgress( int p )
{
  if( !m_onTheFly || m_onlyCreateImage )
    emit subPercent( p );

  if( m_onlyCreateImage )
    emit percent( p );
  else if( !m_onTheFly )
    emit percent( p/2 );
}


void K3bDvdCopyJob::slotReaderProcessedSize( int p, int c )
{
  if( !m_onTheFly || m_onlyCreateImage )
    emit processedSubSize( p, c );

  if( m_onlyCreateImage )
    emit processedSize( p, c );
}


void K3bDvdCopyJob::slotWriterProgress( int p )
{
  emit subPercent( p );

  if( m_onTheFly )
    emit percent( p );
  else
    emit percent( 50 + p/2 );
}


void K3bDvdCopyJob::slotReaderFinished( bool success )
{
  d->readerRunning = false;

  // close the socket
  // otherwise growisofs will never quit.
  // FIXME: is it posiible to do this in a generic manner?
  if( d->writerJob )
    d->writerJob->closeFd();

  // already finished?
  if( !d->running )
    return;
 
  if( d->canceled ) {
    removeImageFiles();
    emit canceled();
    emit finished(false);
    d->running = false;
  }

  if( success ) {
    emit infoMessage( i18n("Successfully read source DVD."), SUCCESS );
    if( m_onlyCreateImage ) {
      emit finished(true);
      d->running = false;
    }
    else if( !m_onTheFly ) {
      if( waitForDvd() ) {
	prepareWriter();
	if( m_copies > 1 )
	  emit newTask( i18n("Writing DVD copy %1").arg(d->doneCopies+1) );
	else
	  emit newTask( i18n("Writing DVD copy") );
	d->writerRunning = true;
	d->writerJob->start();
      }
      else {
	removeImageFiles();
	emit finished(false);
	d->running = false;
      }
    }
  }
  else {
    removeImageFiles();
    emit finished(false);
    d->running = false;
  }
}


void K3bDvdCopyJob::slotWriterFinished( bool success )
{
  d->writerRunning = false;

  // already finished?
  if( !d->running )
    return;

  if( d->canceled ) {
    removeImageFiles();
    emit canceled();
    emit finished(false);
    d->running = false;
  }

  if( success ) {
    d->doneCopies++;

    emit infoMessage( i18n("Successfully written DVD copy %1.").arg(d->doneCopies), INFO );

    if( d->doneCopies < m_copies ) {

      if( waitForDvd() ) {
	prepareWriter();
	emit newTask( i18n("Writing DVD copy %1").arg(d->doneCopies+1) );
	d->writerRunning = true;
	d->writerJob->start();
      }
      else {
	emit finished(false);
	d->running = false;
	return;
      }
      
      if( m_onTheFly ) {
	prepareReader();
	d->readerRunning = true;
	d->dataTrackReader->start();
      }
    }
    else {
      if( m_removeImageFiles )
	removeImageFiles();
      d->running = false;
      emit finished(true);
    }
  }
  else {
    removeImageFiles();
    d->running = false;
    emit finished(false);
  }
}


// this is basicly the same code as in K3bDvdJob... :(
// perhaps this should be moved to some K3bGrowisofsHandler which also parses the growisofs output?
bool K3bDvdCopyJob::waitForDvd()
{
  int mt = 0;
  if( m_writingMode == K3b::WRITING_MODE_INCR_SEQ || m_writingMode == K3b::DAO )
    mt = K3bCdDevice::MEDIA_DVD_RW_SEQ|K3bCdDevice::MEDIA_DVD_R_SEQ;
  else if( m_writingMode == K3b::WRITING_MODE_RES_OVWR ) // we treat DVD+R(W) as restricted overwrite media
    mt = K3bCdDevice::MEDIA_DVD_RW_OVWR|K3bCdDevice::MEDIA_DVD_PLUS_RW|K3bCdDevice::MEDIA_DVD_PLUS_R;
  else
    mt = K3bCdDevice::MEDIA_WRITABLE_DVD;

  int m = waitForMedia( m_writerDevice, K3bCdDevice::STATE_EMPTY, mt );

  if( m < 0 ) {
    cancel();
    return false;
  }
  
  if( m == 0 ) {
    emit infoMessage( i18n("Forced by user. Growisofs will be called without further tests."), INFO );
  }

  else {
    if( m & (K3bCdDevice::MEDIA_DVD_PLUS_RW|K3bCdDevice::MEDIA_DVD_PLUS_R) ) {

      // just for a clean code
      d->usedWritingMode = K3b::WRITING_MODE_RES_OVWR;

      if( m_simulate ) {
	if( KMessageBox::warningYesNo( qApp->activeWindow(),
				       i18n("K3b does not support simulation with DVD+R(W) media. "
					    "Do you really want to continue? The media will actually be "
					    "written to."),
				       i18n("No Simulation with DVD+R(W)") ) == KMessageBox::No ) {
	  cancel();
	  return false;
	}

	m_simulate = false;
      }
      
      if( m_writingMode != K3b::WRITING_MODE_AUTO && m_writingMode != K3b::WRITING_MODE_RES_OVWR )
	emit infoMessage( i18n("Writing mode ignored when writing DVD+R(W) media."), INFO );

      if( m & K3bCdDevice::MEDIA_DVD_PLUS_RW ) {
	emit infoMessage( i18n("Writing DVD+RW."), INFO );
      }
      else
	emit infoMessage( i18n("Writing DVD+R."), INFO );
    }
    else {
      if( m_simulate && !m_writerDevice->dvdMinusTestwrite() ) {
	if( KMessageBox::warningYesNo( qApp->activeWindow(),
				       i18n("Your writer (%1 %2) does not support simulation with DVD-R(W) media. "
					    "Do you really want to continue? The media will be written "
					    "for real.")
				       .arg(m_writerDevice->vendor())
				       .arg(m_writerDevice->description()),
				       i18n("No simulation with DVD-R(W)") ) == KMessageBox::No ) {
	  cancel();
	  return false;
	}

	m_simulate = false;
      }

      //
      // We do not default to DAO in onthefly mode since otherwise growisofs would
      // use the size from the PVD to reserve space on the DVD and that can be bad
      // if this size is wrong
      // With growisofs 5.15 we have the option to specify the size of the image to be written in DAO mode.
      //
      bool sizeWithDao = ( k3bcore->externalBinManager()->binObject( "growisofs" ) && 
			   k3bcore->externalBinManager()->binObject( "growisofs" )->version >= K3bVersion( 5, 15, -1 ) );


      // TODO: check for feature 0x21

      if( m & K3bCdDevice::MEDIA_DVD_RW_OVWR ) {
	emit infoMessage( i18n("Writing DVD-RW in restricted overwrite mode."), INFO );
	d->usedWritingMode = K3b::WRITING_MODE_RES_OVWR;
      }
      else if( m & (K3bCdDevice::MEDIA_DVD_RW_SEQ|
		    K3bCdDevice::MEDIA_DVD_RW) ) {
	if( m_writingMode == K3b::DAO ||
	    ( m_writingMode ==  K3b::WRITING_MODE_AUTO &&
	      ( sizeWithDao || !m_onTheFly ) ) ) {
	  emit infoMessage( i18n("Writing DVD-RW in DAO mode."), INFO );
	  d->usedWritingMode = K3b::DAO;
	}
	else {
	  emit infoMessage( i18n("Writing DVD-RW in sequential mode."), INFO );
	  d->usedWritingMode = K3b::WRITING_MODE_INCR_SEQ;
	}
      }
      else if( m & (K3bCdDevice::MEDIA_DVD_R_SEQ|
		    K3bCdDevice::MEDIA_DVD_R) ) {

	if( m_writingMode == K3b::WRITING_MODE_RES_OVWR )
	  emit infoMessage( i18n("Restricted Overwrite is not possible with DVD-R media."), INFO );

	if( m_writingMode == K3b::DAO ||
	    ( m_writingMode ==  K3b::WRITING_MODE_AUTO &&
	      ( sizeWithDao || !m_onTheFly ) ) ) {
	  emit infoMessage( i18n("Writing DVD-R in DAO mode."), INFO );
	  d->usedWritingMode = K3b::DAO;
	}
	else {
	  emit infoMessage( i18n("Writing DVD-R in sequential mode."), INFO );
	  d->usedWritingMode = K3b::WRITING_MODE_INCR_SEQ;
	}
      }
    }
  }

  return true;
}



void K3bDvdCopyJob::removeImageFiles()
{
  if( QFile::exists( m_imagePath ) ) {
    QFile::remove( m_imagePath );
    emit infoMessage( i18n("Removed image file %1").arg(m_imagePath), K3bJob::SUCCESS );
  }
}


QString K3bDvdCopyJob::jobDescription() const
{
  return i18n("Copying DVD");
}


QString K3bDvdCopyJob::jobDetails() const
{
  if( m_onlyCreateImage )
    return i18n("Creating DVD image");
  else if( m_simulate )
    return i18n("Simulating DVD copy");
  else
    return i18n("Creating 1 DVD copy", "Creating %n DVD copies", m_copies );
}

#include "k3bdvdcopyjob.moc"
