/***************************************************************************
                          k3baudioburndialog.h  -  description
                             -------------------
    begin                : Sun Apr 1 2001
    copyright            : (C) 2001 by Sebastian Trueg
    email                : trueg@informatik.uni-freiburg.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef K3BAUDIOBURNDIALOG_H
#define K3BAUDIOBURNDIALOG_H


#include <kdialogbase.h>

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QGrid;
class QMultiLineEdit;


class K3bAudioDoc;
/**
  *@author Sebastian Trueg
  */

class K3bAudioBurnDialog : public KDialogBase  {

   Q_OBJECT

public:
	K3bAudioBurnDialog(K3bAudioDoc*, QWidget *parent=0, const char *name=0, bool modal = true );
	~K3bAudioBurnDialog();

	enum resultCode { Canceled = 0, Saved = 1, Burn = 2 };

	/** shows the dialog with exec()
		@param burn If true the dialog shows the Burn-button */
	int exec( bool burn );
	
protected:
	void setupBurnTab( QFrame* frame );
	void setupCdTextTab( QFrame* frame );
	
	// the burn tab
	// ---------------------------------------------------------
    QLabel* m_labelCdSize;
    QLabel* m_labelFreeSpace;
    QLineEdit* m_editDirectory;
    QToolButton* m_buttonFindDir;
    QCheckBox* m_checkCdText;
    QCheckBox* m_checkDao;
    QCheckBox* m_checkOnTheFly;
    QCheckBox* m_checkPadding;
    QComboBox* m_comboSpeed;
    QComboBox* m_comboWriter;
    QCheckBox* m_checkSimulate;
	// -----------------------------------------------------------
	
	// the cd-text-tab
	// -----------------------------------------------------------
    QLineEdit* m_editArranger;
    QLineEdit* m_editISRC;
    QLineEdit* m_editArtist;
    QLineEdit* m_editTitle;
    QMultiLineEdit* m_editMessage;
	// -----------------------------------------------------------

private:
	K3bAudioDoc* doc;

protected slots:
  void saveSettings();
  void slotUser1();
  void slotOk();
  void readSettings();
  void slotCancel();
  void slotRefreshWriterSpeeds();
  void slotFindDir();
};

#endif
