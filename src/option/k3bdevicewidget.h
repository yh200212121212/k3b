/*
 *
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2007 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#ifndef K3BDEVICEWIDGET_H
#define K3BDEVICEWIDGET_H

#include <qwidget.h>
#include <QFrame>
#include <QLabel>
#include "k3bdevice.h"
#include "k3bdevicemanager.h"

class QComboBox;
class QLabel;
class Q3GroupBox;
class QPushButton;
class QCheckBox;
class K3bListView;
class QString;
class KIntNumInput;
class QFrame;
class Q3ListViewItem;
class QString;
class QLineEdit;


/**
  *@author Sebastian Trueg
  */
class K3bDeviceWidget : public QWidget
{
  Q_OBJECT

 public:
  K3bDeviceWidget( K3bDevice::DeviceManager*, QWidget *parent = 0 );
  ~K3bDeviceWidget();

 public slots:
  void init();
  void apply();

 signals:
  void refreshButtonClicked();

 private slots:
  void slotNewDevice();
  void updateDeviceListViews();

 private:
  class PrivateTempDevice;
  class PrivateDeviceViewItem1;

  /** list to save changes to the devices before applying */
  Q3PtrList<PrivateTempDevice> m_tempDevices;

  Q3ListViewItem* m_writerParentViewItem;
  Q3ListViewItem* m_readerParentViewItem;

  K3bDevice::DeviceManager* m_deviceManager;

  K3bListView*    m_viewDevices;
  QPushButton* m_buttonRefreshDevices;
  QPushButton* m_buttonAddDevice;
};

#endif
