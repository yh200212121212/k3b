/* 
 *
 * Copyright (C) 2003-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef _K3B_INTERACTION_DIALOG_H_
#define _K3B_INTERACTION_DIALOG_H_

#include <kdialog.h>
#include <QGridLayout>
#include <QEvent>
#include <QLabel>


class QGridLayout;
class QLabel;
class KPushButton;
class K3bThemedHeader;
class KGuiItem;
class QToolButton;
class QEventLoop;


/**
 * This is the base dialog for all the dialogs in K3b that start 
 * some job. Use setMainWidget to set the contents or let mainWidget()
 * create an empty plain page.
 * The default implementations of the slots just emit the 
 * corresponding signals.
 */
class K3bInteractionDialog : public KDialog
{
    Q_OBJECT

public:
    /**
     * The constructor.
     * loadUserDefaults will be called automatically when the dialog is showing.
     *
     * @param title the text to be displayed in the K3b header (not the widget frame)
     * @param subTitle additional text that will be displayed after the title in smaller size
     * @param buttonMask combination of Buttons
     * @param defaultButton may also be null to deactivate the feature
     * @param configgroup The config group used for the loadUserDefaults and saveUserDefaults methods
     */
    K3bInteractionDialog( QWidget* parent = 0, 
                          const QString& title = QString(),
                          const QString& subTitle = QString(),
                          int buttonMask = START_BUTTON|CANCEL_BUTTON,
                          int defaultButton = START_BUTTON,
                          const QString& configgroup = QString() );
    virtual ~K3bInteractionDialog();

    void setMainWidget( QWidget* w );
    void setTitle( const QString& title, const QString& subTitle = QString() );
    void setDefaultButton( int b );

    /**
     * Reimplemented for internal reasons. The API does not change.
     */
    int exec();

    /**
     * reimplemented to allow initialization after the dialog has been opened.
     */
    void show();

    /**
     * If no mainWidget has been set a plain page will be created.
     */
    QWidget* mainWidget();

    enum Buttons {
        START_BUTTON = 1,
        SAVE_BUTTON = 2,
        CANCEL_BUTTON = 4
    };

    QSize sizeHint() const;

    QString configGroup() const { return m_configGroup; }

    /**
     * K3b's dialogs use this method to determine if it is safe to hide when starting
     * some action. Take for example the copy dialog which starts a copy job with a progress
     * dialog. Both the job and the progress dialog are deleted by the copy dialog after the
     * progress dialog has been closed. If the copy dialog would hide itself before starting
     * the job and exitLoopOnHide() would return true the hiding would result in the exec call
     * of the copy dialog to return. And what would that mean for the code after the hide()
     * statement (deleting of the job and so on).
     *
     * \return true in case this dialog will not exit it's private event loop
     *              in case it is hidden.
     *
     * \see exec(bool)
     */
    bool exitLoopOnHide() const { return m_exitLoopOnHide; }

    enum StartUpSettings {
        LOAD_K3B_DEFAULTS = 1,
        LOAD_SAVED_SETTINGS = 2,
        LOAD_LAST_SETTINGS = 3
    };

 Q_SIGNALS:
    void started();
    void canceled();
    void saved();

public Q_SLOTS:
    /**
     * \deprecated use setButtonText
     */
    void setStartButtonText( const QString& text, 
                             const QString& tooltip = QString(), 
                             const QString& whatsthis = QString() );
    /**
     * \deprecated use setButtonText
     */
    void setCancelButtonText( const QString& text, 
                              const QString& tooltip = QString(), 
                              const QString& whatsthis = QString() );
    /**
     * \deprecated use setButtonText
     */
    void setSaveButtonText( const QString& text, 
                            const QString& tooltip = QString(), 
                            const QString& whatsthis = QString() );

    void setButtonGui( int button,
                       const KGuiItem& );

    void setButtonText( int button,
                        const QString& text, 
                        const QString& tooltip = QString(), 
                        const QString& whatsthis = QString() );

    void setButtonEnabled( int button, bool enabled );
    void setButtonShown( int button, bool enabled );

    /**
     * If set true the init() method will be called via a QTimer to ensure event
     * handling be done before (default: false).
     */
    void setDelayedInitialization( bool b ) { m_delayedInit = b; }

    /**
     * Hide the dialog but do not return from the exec call.
     */
    void hideTemporarily();

    /**
     * Close the dialog and return from any exec call.
     */
    void close();

    /**
     * Close the dialog and return from any exec call.
     */
    void done( int r );

protected Q_SLOTS:
    // FIXME: replace these with protected methods which are called from private slots.
    virtual void slotStartClicked();

    /**
     * The default implementation emits the canceled() signal
     * and calls close()
     */
    virtual void slotCancelClicked();
    virtual void slotSaveClicked();

    /**
     * This slot will call the toggleAll() method protecting from infinite loops
     * caused by one element influencing another element which in turn influences
     * the first.
     *
     * Connect this slot to GUI elements (like Checkboxes) that change
     * the state of the whole dialog.
     */
    void slotToggleAll();

protected:
    /**
     * Reimplement this method in case you are using slotToggleAll()
     */
    virtual void toggleAll();

    /**
     * Reimplement this to support the save/load user default buttons.
     * @p config is already set to the correct group.
     *
     * The save/load buttons are only activated if the config group is
     * set in the constructor.
     */
    virtual void saveUserDefaults( KConfigGroup& config );

    /**
     * Reimplement this to support the save/load user default buttons.
     * @p config is already set to the correct group.
     *
     * The save/load buttons are only activated if the config group is
     * set in the constructor.
     */
    virtual void loadUserDefaults( const KConfigGroup& config );

    /**
     * Reimplement this to support the "k3b defaults" button.
     * set all GUI options to reasonable defaults.
     */
    virtual void loadK3bDefaults();

    /**
     * This is called after the dialog has been shown.
     * Use this for initialization that should happen
     * when the user already sees the dialog.
     */
    virtual void init() {}

    /**
     * reimplemented from QDialog
     */
    virtual bool eventFilter( QObject*, QEvent* );

    void hideEvent( QHideEvent* );

private Q_SLOTS:
    void slotLoadK3bDefaults();
    void slotLoadUserDefaults();
    void slotSaveUserDefaults();
    void slotLoadLastSettings();
    void slotStartClickedInternal();
    void slotDelayedInit();

private:
    void initConnections();
    void initToolTipsAndWhatsThis();
    void saveLastSettings();
    void loadStartupSettings();

    KPushButton* getButton( int );

    K3bThemedHeader* m_dialogHeader;
    KPushButton* m_buttonStart;
    KPushButton* m_buttonSave;
    KPushButton* m_buttonCancel;
    QWidget* m_mainWidget;

    QToolButton* m_buttonLoadSettings;
    QToolButton* m_buttonSaveSettings;

    QGridLayout* mainGrid;
    int m_defaultButton;
    QString m_configGroup;

    bool m_exitLoopOnHide;
    bool m_inToggleMode;
    bool m_delayedInit;

    QEventLoop* m_eventLoop;
};

#endif
