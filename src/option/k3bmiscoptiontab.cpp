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


#include "k3bmiscoptiontab.h"

#include <qcheckbox.h>
#include <qfileinfo.h>

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kdialog.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kurlrequester.h>


K3bMiscOptionTab::K3bMiscOptionTab(QWidget *parent, const char *name )
  : base_K3bMiscOptionTab(parent,name)
{
}


K3bMiscOptionTab::~K3bMiscOptionTab()
{
}


void K3bMiscOptionTab::readSettings()
{
  KConfig* c = kapp->config();
  c->setGroup( "General Options" );
  m_checkShowSplash->setChecked( c->readBoolEntry("Show splash", true) );
  m_checkShowSystemTrayProgress->setChecked( c->readBoolEntry( "Show progress in system tray", true ) );
  m_checkHideMainWindowWhileWriting->setChecked( c->readBoolEntry( "hide main window while writing", false ) );

  QString tempdir = c->readEntry( "Temp Dir", KGlobal::dirs()->resourceDirs( "tmp" ).first() );
  m_editTempDir->setURL( tempdir );
}


bool K3bMiscOptionTab::saveSettings()
{
  KConfig* c = kapp->config();
  c->setGroup( "General Options" );
  c->writeEntry( "Show splash", m_checkShowSplash->isChecked() );
  c->writeEntry( "Show progress in system tray", m_checkShowSystemTrayProgress->isChecked() );
  c->writeEntry( "hide main window while writing", m_checkHideMainWindowWhileWriting->isChecked() );

  QString tempDir = m_editTempDir->url();
  QFileInfo fi( tempDir );

  if( fi.isRelative() ) {
    fi.setFile( fi.absFilePath() );
  }

  if( !fi.exists() ) {
    if( KMessageBox::questionYesNo( this, i18n("Directory does not exist. Create?"),
				    i18n("Create Directory") ) == KMessageBox::Yes ) {
      if( !KStandardDirs::makeDir( fi.absFilePath() ) ) {
	KMessageBox::error( this, i18n("Unable to create directory\n(%1)").arg(fi.absFilePath()) );
	return false;
      }
    }
    else {
      // the dir does not exist and the user doesn't want to create it
      return false;
    }
  }

  if( fi.isFile() ) {
    KMessageBox::information( this, i18n("You specified a file for the temporary directory. "
					 "K3b will use its base path as the temporary directory."), 
			      i18n("Warning"), i18n("Don't show again.") );
    fi.setFile( fi.dirPath() );
  }

  // check for writing permission
  if( !fi.isWritable() ) {
    KMessageBox::error( this, i18n("You don't have permission to write to %1.").arg(fi.absFilePath()) );
    return false;
  }

  m_editTempDir->setURL( fi.absFilePath() );

  c->writeEntry( "Temp Dir", m_editTempDir->url() );

  return true;
}


#include "k3bmiscoptiontab.moc"
