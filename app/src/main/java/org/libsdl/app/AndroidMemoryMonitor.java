package org.libsdl.app;

import android.app.Activity;
import android.app.ActivityManager;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Locale;

/** Periodically records process and device memory while the game is visible. */
final class AndroidMemoryMonitor {
    private static final String TAG = "PerimeterMemory";
    private static final long SAMPLE_INTERVAL_MS = 10_000;

    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable sampleTask = new Runnable() {
        @Override
        public void run() {
            if (!running) {
                return;
            }

            logSample();
            handler.postDelayed(this, SAMPLE_INTERVAL_MS);
        }
    };
    private boolean running;
    private boolean reportedSessionMetadata;

    AndroidMemoryMonitor(Activity activity) {
        this.activity = activity;
    }

    void start() {
        if (running) {
            return;
        }

        running = true;
        handler.post(sampleTask);
    }

    void stop() {
        running = false;
        handler.removeCallbacks(sampleTask);
    }

    private void logSample() {
        try {
            ProcessMemoryStats processMemory = readProcessMemoryStats();
            Runtime javaRuntime = Runtime.getRuntime();
            long javaHeapCommittedBytes = javaRuntime.totalMemory();
            long javaHeapUsedBytes = javaHeapCommittedBytes - javaRuntime.freeMemory();

            ActivityManager.MemoryInfo deviceMemory = new ActivityManager.MemoryInfo();
            ActivityManager activityManager =
                    (ActivityManager) activity.getSystemService(Activity.ACTIVITY_SERVICE);
            if (activityManager != null) {
                activityManager.getMemoryInfo(deviceMemory);
            }
            AndroidProcessMemoryStats androidProcessMemory =
                    readAndroidProcessMemoryStats(activityManager);

            if (!reportedSessionMetadata) {
                Log.i(TAG,
                        "session processMemorySource=" + processMemory.source
                                + " systemTotalMiB=" + formatMiBFromBytes(deviceMemory.totalMem)
                                + " systemLowMemoryThresholdMiB="
                                + formatMiBFromBytes(deviceMemory.threshold)
                                + " androidProcessMemorySource=ActivityManager.Debug.MemoryInfo"
                                + " graphicsPssSource=ActivityManager.summary.graphics");
                reportedSessionMetadata = true;
            }

            Log.i(TAG,
                    "elapsedMs=" + SystemClock.elapsedRealtime()
                            + " pssMiB=" + formatMiBFromKb(processMemory.pssKb)
                            + " rssMiB=" + formatMiBFromKb(processMemory.rssKb)
                            + " privateDirtyMiB=" + formatMiBFromKb(processMemory.privateDirtyKb)
                            + " swapPssMiB=" + formatMiBFromKb(processMemory.swapPssKb)
                            + " androidSummaryTotalPssMiB="
                            + formatAndroidSummaryTotalPss(androidProcessMemory, processMemory)
                            + " graphicsPssMiB="
                            + formatMiBFromKb(androidProcessMemory.graphicsPssKb)
                            + " nativeHeapAllocatedMiB="
                            + formatMiBFromBytes(Debug.getNativeHeapAllocatedSize())
                            + " nativeHeapFreeMiB=" + formatMiBFromBytes(Debug.getNativeHeapFreeSize())
                            + " nativeHeapSizeMiB=" + formatMiBFromBytes(Debug.getNativeHeapSize())
                            + " javaHeapUsedMiB=" + formatMiBFromBytes(javaHeapUsedBytes)
                            + " javaHeapCommittedMiB=" + formatMiBFromBytes(javaHeapCommittedBytes)
                            + " systemAvailableMiB=" + formatMiBFromBytes(deviceMemory.availMem)
                            + " lowMemory=" + deviceMemory.lowMemory);
        } catch (RuntimeException exception) {
            Log.w(TAG, "Unable to sample memory usage", exception);
        }
    }

    private static AndroidProcessMemoryStats readAndroidProcessMemoryStats(
            ActivityManager activityManager) {
        if (activityManager == null) {
            return AndroidProcessMemoryStats.UNAVAILABLE;
        }

        try {
            Debug.MemoryInfo[] processMemory =
                    activityManager.getProcessMemoryInfo(new int[]{Process.myPid()});
            if (processMemory == null || processMemory.length == 0) {
                return AndroidProcessMemoryStats.UNAVAILABLE;
            }

            String totalPss = processMemory[0].getMemoryStat("summary.total-pss");
            String graphicsPss = processMemory[0].getMemoryStat("summary.graphics");
            return new AndroidProcessMemoryStats(
                    parseMemoryStatKb(totalPss),
                    parseMemoryStatKb(graphicsPss));
        } catch (RuntimeException exception) {
            return AndroidProcessMemoryStats.UNAVAILABLE;
        }
    }

    private static long parseMemoryStatKb(String value) {
        if (value == null) {
            return -1;
        }
        try {
            return Long.parseLong(value);
        } catch (NumberFormatException exception) {
            return -1;
        }
    }

    private static ProcessMemoryStats readProcessMemoryStats() {
        ProcessMemoryStats stats = new ProcessMemoryStats();
        if (readSmapsRollup(stats)) {
            stats.source = "smaps_rollup";
            return stats;
        }

        if (readStatus(stats)) {
            stats.source = "status";
            return stats;
        }

        stats.source = "unavailable";
        return stats;
    }

    private static boolean readSmapsRollup(ProcessMemoryStats stats) {
        try (BufferedReader reader = new BufferedReader(new FileReader("/proc/self/smaps_rollup"))) {
            String line;
            while ((line = reader.readLine()) != null) {
                int separator = line.indexOf(':');
                if (separator < 0) {
                    continue;
                }

                String key = line.substring(0, separator);
                long valueKb = parseKb(line.substring(separator + 1));
                if ("Pss".equals(key)) {
                    stats.pssKb = valueKb;
                } else if ("Rss".equals(key)) {
                    stats.rssKb = valueKb;
                } else if ("Private_Dirty".equals(key)) {
                    stats.privateDirtyKb = valueKb;
                } else if ("SwapPss".equals(key)) {
                    stats.swapPssKb = valueKb;
                }
            }
            return true;
        } catch (IOException exception) {
            return false;
        }
    }

    private static boolean readStatus(ProcessMemoryStats stats) {
        boolean readAnyValue = false;
        try (BufferedReader reader = new BufferedReader(new FileReader("/proc/self/status"))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.startsWith("VmRSS:")) {
                    stats.rssKb = parseKb(line.substring(line.indexOf(':') + 1));
                    readAnyValue = true;
                }
            }
        } catch (IOException exception) {
            return false;
        }
        return readAnyValue;
    }

    private static long parseKb(String value) {
        String trimmed = value.trim();
        int end = 0;
        while (end < trimmed.length() && Character.isDigit(trimmed.charAt(end))) {
            end++;
        }
        if (end == 0) {
            return -1;
        }
        try {
            return Long.parseLong(trimmed.substring(0, end));
        } catch (NumberFormatException exception) {
            return -1;
        }
    }

    private static String formatMiBFromKb(long valueKb) {
        return valueKb >= 0 ? String.format(Locale.US, "%.1f", valueKb / 1024.0) : "unavailable";
    }

    private static String formatAndroidSummaryTotalPss(
            AndroidProcessMemoryStats androidProcessMemory, ProcessMemoryStats processMemory) {
        if ("smaps_rollup".equals(processMemory.source)
                && androidProcessMemory.totalPssKb >= 0
                && processMemory.pssKb >= 0
                && androidProcessMemory.totalPssKb + 16 * 1024 < processMemory.pssKb) {
            return "unavailable(below_smaps_rollup)";
        }
        return formatMiBFromKb(androidProcessMemory.totalPssKb);
    }

    private static String formatMiBFromBytes(long valueBytes) {
        return valueBytes >= 0
                ? String.format(Locale.US, "%.1f", valueBytes / (1024.0 * 1024.0))
                : "unavailable";
    }

    private static final class ProcessMemoryStats {
        String source;
        long pssKb = -1;
        long rssKb = -1;
        long privateDirtyKb = -1;
        long swapPssKb = -1;
    }

    private static final class AndroidProcessMemoryStats {
        static final AndroidProcessMemoryStats UNAVAILABLE =
                new AndroidProcessMemoryStats(-1, -1);

        final long totalPssKb;
        final long graphicsPssKb;

        AndroidProcessMemoryStats(long totalPssKb, long graphicsPssKb) {
            this.totalPssKb = totalPssKb;
            this.graphicsPssKb = graphicsPssKb;
        }
    }
}
