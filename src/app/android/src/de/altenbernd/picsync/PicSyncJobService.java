// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

package de.altenbernd.picsync;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.os.IBinder;

/**
 * Entry point of the periodic background synchronisation.
 *
 * Android's JobScheduler (configured from C++, see platform_android.cpp)
 * starts this JobService according to the interval, Wi-Fi and charging
 * settings. It only keeps the job alive: it binds the Qt based
 * PicSyncService, which runs the C++ sync engine, and finishes the job as
 * soon as the engine broadcasts that it is done.
 */
public class PicSyncJobService extends JobService
{
    static final String ACTION_FINISHED = "de.altenbernd.picsync.SYNC_FINISHED";

    private JobParameters m_params;
    private boolean m_bound = false;
    private boolean m_receiverRegistered = false;

    /** Keeps the binding to the sync service; no binder is needed. */
    private final ServiceConnection m_connection = new ServiceConnection() {
        /** Called when the sync service is connected (unused). */
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) { }

        /** Called when the sync service process died; the job is rescheduled. */
        @Override
        public void onServiceDisconnected(ComponentName name) { finish(true); }

        /** Called because the Qt service returns no binder; the binding stays active. */
        @Override
        public void onNullBinding(ComponentName name) { }
    };

    /** Receives the "sync finished" broadcast of the C++ engine. */
    private final BroadcastReceiver m_receiver = new BroadcastReceiver() {
        /** Ends the job when the engine reports that it is done. */
        @Override
        public void onReceive(Context context, Intent intent) { finish(false); }
    };

    /** Starts a synchronisation by binding the Qt sync service. */
    @Override
    public boolean onStartJob(JobParameters params)
    {
        m_params = params;
        registerReceiver(m_receiver, new IntentFilter(ACTION_FINISHED),
                         Context.RECEIVER_NOT_EXPORTED);
        m_receiverRegistered = true;
        m_bound = bindService(new Intent(this, PicSyncService.class), m_connection,
                              Context.BIND_AUTO_CREATE);
        if (!m_bound) {
            cleanup();
            return false;
        }
        return true;
    }

    /** Called by Android if the job must stop early; it is retried later. */
    @Override
    public boolean onStopJob(JobParameters params)
    {
        cleanup();
        return true;
    }

    /** Releases the service and reports the end of the job to Android. */
    private void finish(boolean reschedule)
    {
        JobParameters params = m_params;
        cleanup();
        if (params != null)
            jobFinished(params, reschedule);
    }

    /** Unbinds the sync service (which ends its process) and unregisters the receiver. */
    private void cleanup()
    {
        if (m_bound) {
            m_bound = false;
            unbindService(m_connection);
        }
        if (m_receiverRegistered) {
            m_receiverRegistered = false;
            unregisterReceiver(m_receiver);
        }
        m_params = null;
    }
}
