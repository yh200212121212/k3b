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


#ifndef K3BJOB_H
#define K3BJOB_H

#include <qobject.h>
#include "k3bjobhandler.h"
#include "k3b_export.h"

namespace K3bDevice {
    class Device;
}


/**
 * This is the baseclass for all the jobs in K3b which actually do the work like burning a cd!
 * The K3bJob object takes care of registering with the k3bcore or with a parent K3bJob.
 *
 * Every job has a jobhandler which can be another job (in which case the job is handled as
 * a subjob) or an arbitrary class implementing the K3bJobHandler interface.
 *
 * A Job should never create any widgets. User interaction should be done through the methods
 * questionYesNo, waitForMedia.
 *
 * @author Sebastian Trueg
 */
class LIBK3B_EXPORT K3bJob : public QObject, public K3bJobHandler
{
    Q_OBJECT

public:
    virtual ~K3bJob();

    /**
     * \reimplemented from K3bJobHandler
     */
    bool isJob() const { return true; }

    K3bJobHandler* jobHandler() const;

    /**
     * Is the job active?
     * The default implementation is based on the jobStarted() and jobFinished()
     * methods and there is normally no need to reimplement this.
     */
    virtual bool active() const;

    /**
     * The default implementation is based on the canceled() signal.
     *
     * This means that one cannot count on this value being valid
     * in a slot connected to the canceled() signal. It is, however, save
     * to call this method from a slot connected to the finished() signal
     * in case the job makes proper usage of the jobStarted/jobFinished
     * methods.
     */
    virtual bool hasBeenCanceled() const;

    virtual QString jobDescription() const { return "K3bJob"; }
    virtual QString jobDetails() const { return QString(); }

    /**
     * @returns the number of running subjobs.
     * this is useful for proper cancellation of jobs.
     */
    int numRunningSubJobs() const;

    QList<K3bJob*> runningSubJobs() const;

    static const char* DEFAULT_SIGNAL_CONNECTION;

    /**
     * \param newTaskSlot    If DEFAULT_SIGNAL_CONNECTION the newTask signal from the subjob will 
     *                       be connected to the newSubTask signal
     * \param newSubTaskSlot If DEFAULT_SIGNAL_CONNECTION the newSubTask signal from the subjob 
     *                       will create an infoMessage signal
     * \param progressSlot   If DEFAULT_SIGNAL_CONNECTION the percent signal of the subjob will be 
     *                       connected to the subPercent signal.
     * debuggingOutput and infoMessage will always be direcctly connected.
     *
     * If a parameter is set to 0 it will not be connected at all
     */
    virtual void connectSubJob( K3bJob* subJob,
                                const char* finishedSlot = DEFAULT_SIGNAL_CONNECTION,
                                const char* newTaskSlot = DEFAULT_SIGNAL_CONNECTION,
                                const char* newSubTaskSlot = DEFAULT_SIGNAL_CONNECTION,
                                const char* progressSlot = DEFAULT_SIGNAL_CONNECTION,
                                const char* subProgressSlot = DEFAULT_SIGNAL_CONNECTION,
                                const char* processedSizeSlot = DEFAULT_SIGNAL_CONNECTION,
                                const char* processedSubSizeSlot = DEFAULT_SIGNAL_CONNECTION );

    /**
     * Message types to be used in combination with the infoMessage signal.
     *
     * \see infoMessage()
     */
    enum MessageType { 
        INFO,     /**< Informational message. For example a message that informs the user about what is
                     currently going on */
        WARNING,  /**< A warning message. Something did not go perfectly but the job may continue. */
        ERROR,    /**< An error. Only use this message type if the job will actually fail afterwards
                     with a call to jobFinished( false ) */
        SUCCESS   /**< This message type may be used to inform the user that a sub job has 
                     been successfully finished. */
    };

    /**
     * reimplemented from K3bJobHandler
     */
    virtual int waitForMedia( K3bDevice::Device*,
                              int mediaState = K3bDevice::STATE_EMPTY,
                              int mediaType = K3bDevice::MEDIA_WRITABLE_CD,
                              const QString& message = QString() );
  
    /**
     * reimplemented from K3bJobHandler
     */
    virtual bool questionYesNo( const QString& text,
                                const QString& caption = QString(),
                                const QString& yesText = QString(),
                                const QString& noText = QString() );

    /**
     * reimplemented from K3bJobHandler
     */
    virtual void blockingInformation( const QString& text,
                                      const QString& caption = QString() );

    /**
     * Wait for the job to finish. In case the job is not running 
     * this method returns immediately. Otherwise it will start a local
     * event loop to wait for the job to finish.
     */
    void wait();

public Q_SLOTS:
    /**
     * This is the slot that starts the job. The first call should always
     * be jobStarted().
     *
     * Once the job has finished it has to call jobFinished() with the result as
     * a parameter.
     *
     * \see jobStarted()
     * \see jobFinished()
     */
    virtual void start() = 0;

    /**
     * This slot should cancel the job. The job has to emit the canceled() signal and make a call
     * to jobFinished().
     * It is not important to do any of those two directly in this slot though.
     */
    virtual void cancel() = 0;

    void setJobHandler( K3bJobHandler* );

Q_SIGNALS:
    void infoMessage( const QString& msg, int type );
    void percent( int p );
    void subPercent( int p );
    void processedSize( int processed, int size );
    void processedSubSize( int processed, int size );
    void newTask( const QString& job );
    void newSubTask( const QString& job );
    void debuggingOutput(const QString&, const QString&);
    void nextTrack( int track, int numTracks );

    void canceled();

    /**
     * Emitted once the job has been started. Never emit this signal directly.
     * Use jobStarted() instead, otherwise the job will not be properly registered
     */
    void started();

    /**
     * Emitted once the job has been finshed. Never emit this signal directly.
     * Use jobFinished() instead, otherwise the job will not be properly deregistered
     */
    void finished( bool success );

protected:
    /**
     * \param hdl the handler of the job. This allows for some user interaction without
     *            specifying any details (like the GUI).
     *            The job handler can also be another job. In that case this job is a sub job
     *            and will be part of the parents running sub jobs.
     *
     * \see runningSubJobs()
     * \see numRunningSubJobs()
     */
    K3bJob( K3bJobHandler* hdl, QObject* parent = 0 );

    /**
     * Call this in start() to properly register the job and emit the started() signal.
     * Do never emit the started() signal manually.
     *
     * Always call K3bJob::jobStarted in reimplementations.
     */
    virtual void jobStarted();

    /**
     * Call this at the end of the job to properly deregister the job and emit the finished() signal.
     * Do never emit the started() signal manually.
     *
     * Always call K3bJob::jobFinished in reimplementations.
     */
    virtual void jobFinished( bool success );

private Q_SLOTS:
    void slotCanceled();
    void slotNewSubTask( const QString& str );

private:
    void registerSubJob( K3bJob* );
    void unregisterSubJob( K3bJob* );

    class Private;
    Private* const d;
};


/**
 * Every job used to actually burn a medium is derived from K3bBurnJob.
 * This class implements additional signals like buffer status or writing speed
 * as well as a handling of the used writing application.
 */
class LIBK3B_EXPORT K3bBurnJob : public K3bJob
{
    Q_OBJECT
	
public:
    K3bBurnJob( K3bJobHandler* hdl, QObject* parent = 0 );
    virtual ~K3bBurnJob();

    /**
     * The writing device used by this job.
     */	
    virtual K3bDevice::Device* writer() const { return 0; }

    /**
     * use K3b::WritingApp
     */
    int writingApp() const;

    /**
     * K3b::WritingApp "ored" together
     */
    virtual int supportedWritingApps() const;

public Q_SLOTS:
    /**
     * use K3b::WritingApp
     */
    void setWritingApp( int w );

 Q_SIGNALS:
    void bufferStatus( int );

    void deviceBuffer( int );

    /**
     * @param speed current writing speed in Kb
     * @param multiplicator use 150 for CDs and 1380 for DVDs
     * FIXME: maybe one should be able to ask the burnjob if it burns a CD or a DVD and remove the 
     *        multiplicator parameter)
     */
    void writeSpeed( int speed, int multiplicator );

    /**
     * This signal may be used to inform when the burning starts or ends
     * The BurningProgressDialog for example uses it to enable and disable
     * the buffer and writing speed displays.
     */
    void burning(bool);

private:
    class Private;
    Private* const d;
};
#endif
