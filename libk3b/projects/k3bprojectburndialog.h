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


#ifndef K3BPROJECTBURNDIALOG_H
#define K3BPROJECTBURNDIALOG_H

#include <k3binteractiondialog.h>


class K3bDoc;
class K3bBurnJob;
class K3bWriterSelectionWidget;
class K3bTempDirSelectionWidget;
class QGroupBox;
class QCheckBox;
class QTabWidget;
class QSpinBox;
class QVBoxLayout;
class K3bWritingModeWidget;
class KConfig;


/**
  *@author Sebastian Trueg
  */
class K3bProjectBurnDialog : public K3bInteractionDialog
{
   Q_OBJECT

 public:
   K3bProjectBurnDialog( K3bDoc* doc, QWidget *parent=0, const char *name=0, 
			 bool modal = true, bool dvd = false );
   ~K3bProjectBurnDialog();

   enum resultCode { Canceled = 0, Saved = 1, Burn = 2 };

   /** shows the dialog with exec()
       @param burn If true the dialog shows the Burn-button */
   int exec( bool burn );

   K3bDoc* doc() const { return m_doc; }
	
 protected slots:
   /** burn */
   virtual void slotStartClicked();
   /** save */
   virtual void slotSaveClicked();
   virtual void slotCancelClicked();

   /**
    * The default implementation loads the following defaults:
    * <ul>
    *   <li>Writing mode</li>
    *   <li>Simulate</li>
    *   <li>on the fly</li>
    *   <li>remove images</li>
    *   <li>only create images</li>
    * </ul>
    */
   virtual void slotLoadK3bDefaults();

   /**
    * The default implementation loads the following settings from the apps KConfig.
    * It opens the correct group.
    * May be used in subclasses.
    * <ul>
    *   <li>Writing mode</li>
    *   <li>Simulate</li>
    *   <li>on the fly</li>
    *   <li>remove images</li>
    *   <li>only create images</li>
    *   <li>writer</li>
    *   <li>writing speed</li>
    * </ul>
    */
   virtual void slotLoadUserDefaults();

   /**
    * The default implementation saves the following settings to the apps KConfig.
    * It opens the correct group.
    * May be used in subclasses.
    * <ul>
    *   <li>Writing mode</li>
    *   <li>Simulate</li>
    *   <li>on the fly</li>
    *   <li>remove images</li>
    *   <li>only create images</li>
    *   <li>writer</li>
    *   <li>writing speed</li>
    * </ul>
    */
   virtual void slotSaveUserDefaults();

   /**
    * gets called if the user changed the writer
    * default implementation just calls 
    * toggleAllOptions()
    */
   virtual void slotWriterChanged();

   /**
    * gets called if the user changed the writing app
    * default implementation just calls 
    * toggleAllOptions()
    */
   virtual void slotWritingAppChanged( int );

   virtual void toggleAllOptions();

 signals:
   void writerChanged();

 protected:
   /**
    * The default implementation saves the following settings to the doc and may be called 
    * in subclasses:
    * <ul>
    *   <li>Writing mode</li>
    *   <li>Simulate</li>
    *   <li>on the fly</li>
    *   <li>remove images</li>
    *   <li>only create images</li>
    *   <li>writer</li>
    *   <li>writing speed</li>
    * </ul>
    */
   virtual void saveSettings();

   /**
    * The default implementation reads the following settings from the doc and may be called 
    * in subclasses:
    * <ul>
    *   <li>Writing mode</li>
    *   <li>Simulate</li>
    *   <li>on the fly</li>
    *   <li>remove images</li>
    *   <li>only create images</li>
    *   <li>writer</li>
    *   <li>writing speed</li>
    * </ul>
    */
   virtual void readSettings();

   /**
    * use this to set additionell stuff in the job
    */
   virtual void prepareJob( K3bBurnJob* ) {};

   void prepareGui();
   void addPage( QWidget*, const QString& title );

   K3bWriterSelectionWidget* m_writerSelectionWidget;
   K3bTempDirSelectionWidget* m_tempDirSelectionWidget;
   K3bWritingModeWidget* m_writingModeWidget;
   QGroupBox* m_optionGroup;
   QVBoxLayout* m_optionGroupLayout;
   QCheckBox* m_checkOnTheFly;
   QCheckBox* m_checkSimulate;
   QCheckBox* m_checkRemoveBufferFiles;
   QCheckBox* m_checkOnlyCreateImage;
   QSpinBox* m_spinCopies;

 private:
   K3bDoc* m_doc;
   K3bBurnJob* m_job;
   QTabWidget* m_tabWidget;
   bool m_dvd;
};

#endif
